#include <kernel/interrupts/irq.h>
#include <kernel/interrupts/idt.h>
#include <kernel/interrupts/pic.h>

/* Обработчики для линий PIC 0-15; NULL — IRQ игнорируется (EOI всё равно отправится). */
static irq_handler_t irq_handlers[16];

void irq_register(uint8_t irq, irq_handler_t handler) {
	if (irq >= 16) {
		return;
	}
	irq_handlers[irq] = handler;
}

/*
 * Общий обработчик аппаратных прерываний (вызывается из irq_common_stub).
 * regs->int_no содержит номер IRQ (0-15), не вектор IDT.
 */
void irq_handler(registers_t* regs) {
	uint8_t irq = (uint8_t)regs->int_no;

	if (irq < 16 && irq_handlers[irq] != 0) {
		irq_handlers[irq](regs);
	}

	/* Без EOI PIC не пришлёт следующее прерывание на этой линии. */
	pic_send_eoi(irq);
}

/*
 * Связывает IRQ с векторами IDT:
 * IRQ 0 -> INT 32, IRQ 1 -> INT 33, …, IRQ 15 -> INT 47.
 */
void irq_init(void) {
	for (uint8_t i = 0; i < 16; i++) {
		idt_set_descriptor((uint8_t)(32 + i), irq_stub_table[i], 0x8E);
	}
}
