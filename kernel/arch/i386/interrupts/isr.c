#include <kernel/interrupts/isr.h>
#include <kernel/tty.h>
#include "../../../kernel/arch/i386/vga.h"

void exception_handler(registers_t* regs) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    terminal_writestring("EXCEPTION! ");
    
    // Выводим информацию об исключении
    char* exception_messages[] = {
        "Division By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Into Detected Overflow",
        "Out of Bounds",
        "Invalid Opcode",
        "No Coprocessor",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Bad TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault",
        "Unknown Interrupt",
        "Coprocessor Fault",
        "Alignment Check",
        "Machine Check",
        "SIMD Floating-Point Exception",
        "Virtualization Exception",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Security Exception",
        "Reserved"
    };
    
    if (regs->int_no < 32) {
        terminal_writestring(exception_messages[regs->int_no]);
    }
    
    // Бесконечный цикл
    while(1) {
        __asm__ volatile ("cli; hlt");
    }
}