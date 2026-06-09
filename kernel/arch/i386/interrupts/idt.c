#include <kernel/interrupts/idt.h>
#include <stdbool.h>
#include <string.h>

__attribute__((aligned(0x10))) static idt_entry_t idt[256]; // Create an array of IDT entries; aligned for performance
static idtr_t idtr;
static bool vectors[IDT_MAX_DESCRIPTORS];
extern void* isr_stub_table[];

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];
    uintptr_t addr = (uintptr_t)isr;

    descriptor->isr_low        = (uint16_t)(addr & 0xFFFF);
    descriptor->kernel_cs      = 0x08; // this value must match your kernel code selector in the GDT
    descriptor->attributes     = flags;
    descriptor->isr_high       = (uint16_t)((addr >> 16) & 0xFFFF);
    descriptor->reserved       = 0;
}

void idt_init() {
    memset(idt, 0, sizeof(idt));

    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1);

    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
    }

    __asm__ volatile ("lidt %0" : : "m"(idtr));
}