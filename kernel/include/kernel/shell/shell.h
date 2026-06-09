#ifndef KERNEL_SHELL_SHELL_H
#define KERNEL_SHELL_SHELL_H

/*
 * Простой интерактивный терминал.
 * Prompt, echo ввода, Backspace, Enter.
 * Команды пока не выполняются — только набор строки в shell_line.
 */
void shell_run(void);

#endif
