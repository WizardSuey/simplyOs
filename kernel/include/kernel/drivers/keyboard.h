#ifndef KERNEL_DRIVERS_KEYBOARD_H
#define KERNEL_DRIVERS_KEYBOARD_H

#include <stdbool.h>

/*
 * Драйвер PS/2-клавиатуры.
 * IRQ 1 -> scancode из порта 0x60 -> ASCII в кольцевой буфер.
 * Поддерживает Shift, Caps Lock; Ctrl/Alt и extended-клавиши пока игнорируются.
 * Вызывать keyboard_init() после irq_init() и pic_init(), до sti.
 */

/* Подключает обработчик IRQ 1 и разрешает линию в PIC. */
void keyboard_init(void);

/* true, если в буфере есть хотя бы один символ. */
bool keyboard_has_key(void);

/*
 * Блокирующее чтение символа из буфера.
 * Если буфер пуст — CPU засыпает (hlt) до следующего нажатия.
 */
char keyboard_getchar(void);

#endif
