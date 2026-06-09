#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/mm/multiboot.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/kheap.h>
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

static void paging_self_test(void) {
	/* Свободный виртуальный адрес выше boot map (0xC03FFFFF). */
	const uintptr_t test_virt = 0xC1000000u;
	const uint32_t magic = 0xDEADBEEFu;
	void* frame;

	frame = pmm_alloc_frame();
	if (frame == 0) {
		printf("Paging self-test: alloc failed\n");
		return;
	}

	if (!paging_map_page(test_virt, (uintptr_t)frame, PAGING_FLAG_WRITE)) {
		printf("Paging self-test: map failed\n");
		pmm_free_frame(frame);
		return;
	}

	*(volatile uint32_t*)test_virt = magic;
	if (*(volatile uint32_t*)test_virt != magic) {
		printf("Paging self-test: read/write failed\n");
		paging_unmap_page(test_virt);
		pmm_free_frame(frame);
		return;
	}

	if (paging_get_physical(test_virt) != (uintptr_t)frame) {
		printf("Paging self-test: phys mismatch\n");
		paging_unmap_page(test_virt);
		pmm_free_frame(frame);
		return;
	}

	paging_unmap_page(test_virt);
	if (paging_get_physical(test_virt) != 0) {
		printf("Paging self-test: unmap failed\n");
		pmm_free_frame(frame);
		return;
	}

	pmm_free_frame(frame);
	printf("Paging self-test: ok\n");
}

static void kheap_self_test(void) {
	char* a;
	char* b;

	a = kmalloc(64);
	b = kmalloc(128);
	if (a == NULL || b == NULL) {
		printf("Kheap self-test: alloc failed\n");
		return;
	}

	if (a == b) {
		printf("Kheap self-test: overlap\n");
		kfree(a);
		kfree(b);
		return;
	}

	a[0] = 'A';
	a[63] = 'Z';
	b[0] = 'B';
	b[127] = 'Y';

	if (a[0] != 'A' || a[63] != 'Z' || b[0] != 'B' || b[127] != 'Y') {
		printf("Kheap self-test: read/write failed\n");
		kfree(a);
		kfree(b);
		return;
	}

	kfree(a);
	kfree(b);
	printf("Kheap self-test: ok (%u / %u bytes)\n",
		(unsigned)kheap_used_bytes(), (unsigned)kheap_total_bytes());
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

	paging_init();
	paging_self_test();

	kheap_init();
	kheap_self_test();

	printf("Page fault handler: ready\n");

	keyboard_init();
	__asm__ volatile ("sti");
	
	shell_run();
}
