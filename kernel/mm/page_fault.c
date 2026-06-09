#include <kernel/mm/page_fault.h>
#include <kernel/mm/paging.h>
#include <kernel/tty.h>
#include <stdio.h>

#include "../arch/i386/vga.h"

#define PAGE_FAULT_VECTOR 14u

/* Читает CR2 — виртуальный адрес, вызвавший #PF. */
static inline uint32_t read_cr2(void) {
	uint32_t cr2;
	__asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
	return cr2;
}

/*
 * Error code #PF (bits):
 *  0 (P)  — 0: страница not present; 1: protection (present, но доступ запрещён)
 *  1 (W/R)— 0: read; 1: write
 *  2 (U/S)— 0: supervisor (ring 0); 1: user (ring 3)
 *  3 (RSVD)— reserved bit в PTE установлен в 1
 *  4 (I/D)— instruction fetch (если CPU поддерживает NX через PTE)
 */
static void page_fault_print_error_code(uint32_t err) {
	printf("  cause:");

	if ((err & 0x1) == 0) {
		printf(" not-present");
	} else {
		printf(" protection-violation");
	}

	if (err & 0x2) {
		printf(" write");
	} else {
		printf(" read");
	}

	if (err & 0x4) {
		printf(" user-mode");
	} else {
		printf(" supervisor");
	}

	if (err & 0x8) {
		printf(" reserved-bit-set");
	}

	if (err & 0x10) {
		printf(" instruction-fetch");
	}

	printf(" (err=0x%x)\n", err);
}

__attribute__((noreturn))
void page_fault_handler(registers_t* regs) {
	uint32_t cr2 = read_cr2();
	uintptr_t mapped_phys;

	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));

	printf("Page Fault (#%u)\n", PAGE_FAULT_VECTOR);
	printf("  CR2 (fault addr): %p\n", (void*)(uintptr_t)cr2);
	page_fault_print_error_code(regs->err_code);

	printf("  EIP: %p  CS: 0x%x\n", (void*)(uintptr_t)regs->eip, regs->cs);
	printf("  EAX: %p  EBX: %p  ECX: %p  EDX: %p\n",
		(void*)(uintptr_t)regs->eax,
		(void*)(uintptr_t)regs->ebx,
		(void*)(uintptr_t)regs->ecx,
		(void*)(uintptr_t)regs->edx);

	mapped_phys = paging_get_physical(cr2);
	if (mapped_phys != 0) {
		printf("  paging: mapped -> phys %p\n", (void*)mapped_phys);
	} else {
		printf("  paging: not mapped in page tables\n");
	}

	for (;;) {
		__asm__ volatile ("cli; hlt");
	}
}
