#include <kernel/interrupts/pic.h>
#include <kernel/io/io.h>

// Порт команд master PIC (основной контроллер прерываний 8259A)
#define PIC1_COMMAND 0x20     // Порт для передачи команд главному (master) PIC

// Порт данных master PIC — здесь устанавливаются маски/значения для master PIC
#define PIC1_DATA    0x21     // Порт для передачи/чтения данных главному (master) PIC

// Порт команд slave PIC (дополнительный контроллер прерываний 8259A)
#define PIC2_COMMAND 0xA0     // Порт для передачи команд вторичному (slave) PIC

// Порт данных slave PIC — здесь устанавливаются маски/значения для slave PIC
#define PIC2_DATA    0xA1     // Порт для передачи/чтения данных вторичному (slave) PIC

// Флаг для ICW1: начать инициализацию PIC (должен быть выставлен для начала конфигурирования)
#define ICW1_INIT    0x10     // Бит 4 (1) — запуск последовательности инициализации

// Флаг для ICW1: требуется ICW4 (указать, что далее будет отправлен четвертый управляющий байт ICW4)
#define ICW1_ICW4    0x01     // Бит 0 (1) — сообщить о необходимости ICW4

// Флаг для ICW4: задать режим работы 8086/88, а не MCS-80/85 (режим по умолчанию для большинства ПК)
#define ICW4_8086    0x01     // Бит 0 (1) — выбор режима 8086/88 (Microprocessor mode)

// Команда End Of Interrupt: отправить PIC сигнал о том, что обработка прерывания завершена
#define PIC_EOI      0x20     // Бит 5 (1) — команда "End Of Interrupt"

/* Возвращает порт маски (DATA) для IRQ 0-7 или 8-15. */
static uint16_t pic_mask_port(uint8_t irq) {
	return irq < 8 ? PIC1_DATA : PIC2_DATA;
}

/* Номер линии внутри контроллера: 0-7 для master, 0-7 для slave. */
static uint8_t pic_irq_line(uint8_t irq) {
	return irq < 8 ? irq : (uint8_t)(irq - 8);
}

/*
 * Переназначает IRQ 0-15 на INT 32-47 и маскирует все линии.
 * BIOS по умолчанию мапит IRQ на INT 8-15, что конфликтует с исключениями CPU.
 */
void pic_init(void) {
	/* ICW1: master и slave — старт инициализации. */
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();

	/* ICW2: смещение векторов в IDT. */
	outb(PIC1_DATA, PIC1_OFFSET); /* master: IRQ 0 -> INT 32 */
	io_wait();
	outb(PIC2_DATA, PIC2_OFFSET); /* slave:  IRQ 8 -> INT 40 */
	io_wait();

	/* ICW3: slave подключён к IRQ2 master; slave сообщает свой ID = 2. */
	outb(PIC1_DATA, 4);
	io_wait();
	outb(PIC2_DATA, 2);
	io_wait();

	/* ICW4: 8086-режим на обоих контроллерах. */
	outb(PIC1_DATA, ICW4_8086);
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();

	/* Маскируем все IRQ, пока не зарегистрируем обработчики в IDT. */
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
}

/*
 * Отправляет EOI после обработки IRQ.
 * Для IRQ >= 8 нужно подтвердить и slave, и master.
 */
void pic_send_eoi(uint8_t irq) {
	if (irq >= 8) {
		outb(PIC2_COMMAND, PIC_EOI);
	}
	outb(PIC1_COMMAND, PIC_EOI);
}

/* Запрещает прерывание на линии irq (бит маски = 1). */
void pic_mask(uint8_t irq) {
	uint16_t port = pic_mask_port(irq);
	uint8_t line = pic_irq_line(irq);
	outb(port, (uint8_t)(inb(port) | (1u << line)));
}

/* Разрешает прерывание на линии irq (бит маски = 0). */
void pic_unmask(uint8_t irq) {
	uint16_t port = pic_mask_port(irq);
	uint8_t line = pic_irq_line(irq);
	outb(port, (uint8_t)(inb(port) & ~(1u << line)));
}
