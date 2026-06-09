#include <stddef.h>

#include <kernel/drivers/keyboard.h>
#include <kernel/interrupts/irq.h>
#include <kernel/interrupts/pic.h>
#include <kernel/io/io.h>

/* Порт данных PS/2-контроллера: сюда приходит scancode после IRQ. */
#define KBD_DATA_PORT 0x60

/* Линия PIC для клавиатуры; после remap -> INT 33. */
#define KBD_IRQ       1

/* Кольцевой буфер между IRQ-обработчиком и кодом ядра (shell). */
#define KBD_BUF_SIZE  256

/* Scancode set 1 — модификаторы и префикс расширенных кодов. */
#define SCANCODE_LSHIFT 0x2A
#define SCANCODE_RSHIFT 0x36
#define SCANCODE_CAPS   0x3A
#define SCANCODE_LCTRL  0x1D
#define SCANCODE_LALT   0x38
#define SCANCODE_EXT    0xE0  /* следующий байт — extended scancode (стрелки и т.д.) */

/*
 * Scan code set 1 -> ASCII, US QWERTY, без Shift.
 * Индекс = scancode нажатия; 0 = не печатаем (Shift, Ctrl, F-клавиши…).
 * 27 = Esc (управляющий символ ESC, не цифра «27»).
 */
static const char scancode_ascii[128] = {
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
	0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
	0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
	'*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* Та же раскладка при зажатом Shift: !@#, заглавные буквы, { } и т.д. */
static const char scancode_ascii_shift[128] = {
	0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
	0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
	0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
	'*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static char kbd_buffer[KBD_BUF_SIZE];
static volatile size_t kbd_read_pos;
static volatile size_t kbd_write_pos;

/* Состояние модификаторов между нажатием и отпусканием. */
static bool left_shift;
static bool right_shift;
static bool caps_lock;
static bool extended;  /* true после байта 0xE0, до следующего scancode */

/* Scancode буквенных рядов Q–P, A–L, Z–M (для Caps Lock только буквы). */
static bool is_letter_scancode(uint8_t scancode) {
	return (scancode >= 0x10 && scancode <= 0x19)
		|| (scancode >= 0x1E && scancode <= 0x26)
		|| (scancode >= 0x2C && scancode <= 0x32);
}

/*
 * Выбирает таблицу по Shift/Caps:
 * - буквы: shift XOR caps_lock -> верхний/нижний регистр;
 * - цифры и знаки: только Shift.
 */
static char scancode_to_ascii(uint8_t scancode) {
	const char* table = scancode_ascii;
	bool shift = left_shift || right_shift;

	if (scancode >= sizeof(scancode_ascii)) {
		return 0;
	}

	if (is_letter_scancode(scancode)) {
		if (shift ^ caps_lock) {
			table = scancode_ascii_shift;
		}
	} else if (shift) {
		table = scancode_ascii_shift;
	}

	return table[scancode];
}

/* Запись в кольцевой буфер; при переполнении символ отбрасывается. */
static void kbd_buffer_push(char c) {
	size_t next = (kbd_write_pos + 1) % KBD_BUF_SIZE;
	if (next == kbd_read_pos) {
		return;
	}
	kbd_buffer[kbd_write_pos] = c;
	kbd_write_pos = next;
}

/*
 * IRQ-обработчик клавиатуры.
 * EOI отправляется в irq_handler после return отсюда.
 */
static void keyboard_irq_handler(registers_t* regs) {
	uint8_t scancode;
	char c;

	(void)regs;

	scancode = inb(KBD_DATA_PORT);

	/* Префикс extended: стрелки, Insert, Delete и т.п. — пока игнорируем. */
	if (scancode == SCANCODE_EXT) {
		extended = true;
		return;
	}

	/* Старший бит = отпускание (break code). */
	if (scancode & 0x80) {
		scancode &= 0x7F;
		if (scancode == SCANCODE_LSHIFT) {
			left_shift = false;
		} else if (scancode == SCANCODE_RSHIFT) {
			right_shift = false;
		}
		extended = false;
		return;
	}

	if (extended) {
		extended = false;
		return;
	}

	switch (scancode) {
	case SCANCODE_LSHIFT:
		left_shift = true;
		return;
	case SCANCODE_RSHIFT:
		right_shift = true;
		return;
	case SCANCODE_CAPS:
		caps_lock = !caps_lock;
		return;
	case SCANCODE_LCTRL:
	case SCANCODE_LALT:
		return;
	default:
		break;
	}

	c = scancode_to_ascii(scancode);
	if (c != 0) {
		kbd_buffer_push(c);
	}
}

void keyboard_init(void) {
	kbd_read_pos = 0;
	kbd_write_pos = 0;
	left_shift = false;
	right_shift = false;
	caps_lock = false;
	extended = false;
	irq_register(KBD_IRQ, keyboard_irq_handler);
	pic_unmask(KBD_IRQ);
}

bool keyboard_has_key(void) {
	return kbd_read_pos != kbd_write_pos;
}

/*
 * Блокирующее чтение: cli при проверке буфера, hlt если пусто.
 * IRQ клавиатуры разбудит CPU после следующего нажатия.
 */
char keyboard_getchar(void) {
	char c;

	while (1) {
		__asm__ volatile ("cli");
		if (kbd_read_pos != kbd_write_pos) {
			c = kbd_buffer[kbd_read_pos];
			kbd_read_pos = (kbd_read_pos + 1) % KBD_BUF_SIZE;
			__asm__ volatile ("sti");
			return c;
		}
		__asm__ volatile ("sti");
		__asm__ volatile ("hlt");
	}
}
