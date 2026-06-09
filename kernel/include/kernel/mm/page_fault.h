#ifndef KERNEL_MM_PAGE_FAULT_H
#define KERNEL_MM_PAGE_FAULT_H

#include <kernel/interrupts/isr.h>

/* Обработчик #PF (int 14): CR2, error code, контекст; не возвращается. */
__attribute__((noreturn))
void page_fault_handler(registers_t* regs);

#endif
