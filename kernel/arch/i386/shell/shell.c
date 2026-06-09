#include <kernel/shell/shell.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/tty.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/kheap.h>
#include <stdint.h>
#include <stdio.h>

/* Максимальная длина вводимой строки (без завершающего '\0'). */
#define SHELL_LINE_MAX 128

/*
 * Текущая строка ввода.
 * После Enter здесь будет текст команды — передаётся в shell_execute.
 */
static char shell_line[SHELL_LINE_MAX];
static size_t shell_line_len;

/* Допускаем печатаемый ASCII и Tab; Esc и прочие управляющие — нет. */
static bool shell_is_printable(char c) {
	unsigned char uc = (unsigned char)c;
	return uc == '\t' || (uc >= 32 && uc <= 126);
}

static bool shell_streq(const char* a, const char* b) {
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return false;
		}
		a++;
		b++;
	}
	return *a == *b;
}

static void shell_prompt(void) {
	terminal_writestring("myos> ");
}

static void shell_reset_line(void) {
	shell_line_len = 0;
	shell_line[0] = '\0';
}

static void shell_cmd_help(void) {
	printf("Commands:\n");
	printf("  help   - this list\n");
	printf("  mem    - PMM and kheap statistics\n");
	printf("  pfault - trigger page fault (halts kernel)\n");
}

static void shell_cmd_mem(void) {
	printf("PMM:  %u free / %u frames (%u used)\n",
		pmm_free_frames(), pmm_total_frames(), pmm_used_frames());
	printf("Kheap: %u used / %u bytes mapped\n",
		(unsigned)kheap_used_bytes(), (unsigned)kheap_total_bytes());
}

/*
 * Намеренный page fault для проверки page_fault_handler.
 * После вызова ядро зависнет — это ожидаемо.
 */
static void shell_cmd_pfault(void) __attribute__((noreturn));

static void shell_cmd_pfault(void) {
	printf("Triggering page fault at 0xDEAD0000...\n");
	*(volatile uint32_t*)0xDEAD0000;

	for (;;) {
		__asm__ volatile ("cli; hlt");
	}
}

static void shell_execute(const char* line) {
	if (line[0] == '\0') {
		return;
	}

	if (shell_streq(line, "help")) {
		shell_cmd_help();
		return;
	}

	if (shell_streq(line, "mem")) {
		shell_cmd_mem();
		return;
	}

	if (shell_streq(line, "pfault")) {
		shell_cmd_pfault();
	}

	printf("Unknown command: %s\n", line);
	printf("Type 'help' for a list of commands.\n");
}

/*
 * Главный цикл терминала: prompt, echo, разбор команд по Enter.
 */
void shell_run(void) {
	terminal_writestring("My-OS shell. Type 'help' for commands.\n");
	shell_reset_line();

	for (;;) {
		shell_prompt();

		while (1) {
			char c = keyboard_getchar();

			if (c == '\n') {
				terminal_putchar('\n');
				shell_line[shell_line_len] = '\0';
				shell_execute(shell_line);
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

			shell_line[shell_line_len++] = c;
			shell_line[shell_line_len] = '\0';
			terminal_putchar(c);
		}
	}
}
