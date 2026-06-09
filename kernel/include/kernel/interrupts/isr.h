#ifndef KERNEL_INTERRUPTS_ISR_H
#define KERNEL_INTERRUPTS_ISR_H

#include <stdint.h>

// Структура для доступа к регистрам из стека
typedef struct {
    uint32_t gs, fs, es, ds;      // Сегменты (pushw расширяется до 32 бит)
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // pusha
    uint32_t int_no, err_code;    // Номер прерывания и код ошибки
    uint32_t eip, cs, eflags, useresp, ss;  // Аппаратно сохраняемые
} __attribute__((packed)) registers_t;

__attribute__((noreturn))
void exception_handler(registers_t* regs);

#endif
