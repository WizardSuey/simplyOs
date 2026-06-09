#include <kernel/shell/shell.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/tty.h>

/* Максимальная длина вводимой строки (без завершающего '\0'). */
#define SHELL_LINE_MAX 128

/*
 * Текущая строка ввода.
 * После Enter здесь будет текст команды — позже можно передать в парсер.
 */
static char shell_line[SHELL_LINE_MAX];
static size_t shell_line_len;

/* Допускаем печатаемый ASCII и Tab; Esc и прочие управляющие — нет. */
static bool shell_is_printable(char c) {
	unsigned char uc = (unsigned char)c;
	return uc == '\t' || (uc >= 32 && uc <= 126);
}

static void shell_prompt(void) {
	terminal_writestring("myos> ");
}

static void shell_reset_line(void) {
	shell_line_len = 0;
	shell_line[0] = '\0';
}

/*
 * Главный цикл простого терминала.
 * Пока только echo: ввод -> экран, Enter -> новая строка и снова prompt.
 * Выполнение команд будет добавлено позже.
 */
void shell_run(void) {
	terminal_writestring("My-OS terminal. Type text and press Enter.\n");
	shell_reset_line();

	for (;;) {
		shell_prompt();

		/* Читаем символы до Enter — одна «команда» (строка). */
		while (1) {
			char c = keyboard_getchar();

			if (c == '\n') {
				terminal_putchar('\n');
				shell_line[shell_line_len] = '\0';
				/* TODO: разбор shell_line и выполнение команды. */
				shell_reset_line();
				break;
			}

			if (c == '\b') {
				if (shell_line_len > 0) {
					shell_line_len--;
					shell_line[shell_line_len] = '\0';
					terminal_putchar('\b');
				}
				continue;
			}

			if (!shell_is_printable(c)) {
				continue;
			}

			if (shell_line_len >= SHELL_LINE_MAX - 1) {
				continue;
			}

			/* Echo: символ и в буфер строки, и на экран. */
			shell_line[shell_line_len++] = c;
			shell_line[shell_line_len] = '\0';
			terminal_putchar(c);
		}
	}
}
