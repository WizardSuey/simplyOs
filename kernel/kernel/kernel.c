#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/mm/multiboot.h>
#include <kernel/mm/pmm.h>
#include <kernel/interrupts/idt.h>
#include <kernel/interrupts/pic.h>
#include <kernel/interrupts/irq.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/shell/shell.h>
#include <kernel/gdt/gdt.h>

static void pmm_self_test(void) {
	void* a = pmm_alloc_frame();
	void* b = pmm_alloc_frame();

	if (a == 0 || b == 0) {
		printf("PMM self-test: alloc failed\n");
		return;
	}

	if (a == b) {
		printf("PMM self-test: duplicate frame %p\n", a);
		return;
	}

	pmm_free_frame(a);
	pmm_free_frame(b);
	printf("PMM self-test: ok\n");
}

void kernel_main(void) {
	terminal_initialize();

	gdt_init();
	idt_init();
	irq_init();
	pic_init();

	if (!multiboot_is_valid()) {
		printf("Multiboot: memory map unavailable\n");
	} else {
		printf("Multiboot: %u mmap regions\n", multiboot_mmap_entry_count());
	}

	pmm_init();
	printf("PMM: %u free / %u frames (%u used)\n",
		pmm_free_frames(), pmm_total_frames(), pmm_used_frames());
	pmm_self_test();

	keyboard_init();

	__asm__ volatile ("sti");

	shell_run();
}
