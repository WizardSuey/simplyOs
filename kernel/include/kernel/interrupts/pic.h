#ifndef KERNEL_INTERRUPTS_PIC_H
#define KERNEL_INTERRUPTS_PIC_H

#include <stdint.h>

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);

#endif
