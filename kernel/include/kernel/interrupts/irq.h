#ifndef KERNEL_INTERRUPTS_IRQ_H
#define KERNEL_INTERRUPTS_IRQ_H

#include <stdint.h>
#include <kernel/interrupts/isr.h>

/* C-обработчик аппаратного IRQ; вызывается из irq_handler после сохранения регистров. */
typedef void (*irq_handler_t)(registers_t* regs);

/* Регистрирует в IDT заглушки для векторов 32-47 (IRQ 0-15 после remap PIC). */
void irq_init(void);

/* Назначает обработчик для линии PIC: irq 0-15 (например, 1 — клавиатура). */
void irq_register(uint8_t irq, irq_handler_t handler);

/* Таблица адресов asm-заглушек irq_stub_0 … irq_stub_15 (см. irq_stubs.S). */
extern void* irq_stub_table[];

#endif
