#include <kernel/mm/pmm.h>
#include <kernel/mm/multiboot.h>
#include <string.h>

/* Higher-half: виртуальный адрес ядра = физический + 0xC0000000. */
#define KERNEL_VIRTUAL_BASE 0xC0000000
/* Ядро загружается GRUB по физическому адресу 1 MiB (linker.ld). */
#define KERNEL_PHYSICAL_START 0x00100000u

/* До 128 MiB RAM: 32768 фреймов по 4 KiB, bitmap занимает 4 KiB. */
#define PMM_MAX_FRAMES 32768

extern char _kernel_end[];
extern char boot_page_directory[];
extern char boot_page_table1[];
extern char stack_bottom[];
extern char stack_top[];

/* 1 бит = 1 физический фрейм: 1 — занят, 0 — свободен. */
static uint8_t pmm_bitmap[PMM_MAX_FRAMES / 8];
static uint32_t pmm_frame_count;
static uint32_t pmm_used_count;

/* Переводит физический адрес в номер фрейма (страница 4096 байт). */
static inline uint32_t addr_to_frame(uintptr_t addr) {
	return (uint32_t)(addr / PMM_PAGE_SIZE);
}

/*
 * Меняет состояние одного фрейма в bitmap и поддерживает счётчик pmm_used_count.
 * Выход за pmm_frame_count игнорируется — защита от переполнения bitmap.
 */
static inline void pmm_set_used(uint32_t frame, bool used) {
	if (frame >= pmm_frame_count) {
		return;
	}

	if (used) {
		if ((pmm_bitmap[frame / 8] & (1u << (frame % 8))) == 0) {
			pmm_used_count++;
		}
		pmm_bitmap[frame / 8] |= (1u << (frame % 8));
	} else {
		if (pmm_bitmap[frame / 8] & (1u << (frame % 8))) {
			pmm_used_count--;
		}
		pmm_bitmap[frame / 8] &= ~(1u << (frame % 8));
	}
}

/* Помечает диапазон фреймов [start_frame, end_frame) как занятые. */
static void pmm_reserve_frames(uint32_t start_frame, uint32_t end_frame) {
	for (uint32_t frame = start_frame; frame < end_frame && frame < pmm_frame_count; frame++) {
		pmm_set_used(frame, true);
	}
}

/* Помечает диапазон фреймов [start_frame, end_frame) как свободные. */
static void pmm_free_frames_range(uint32_t start_frame, uint32_t end_frame) {
	for (uint32_t frame = start_frame; frame < end_frame && frame < pmm_frame_count; frame++) {
		pmm_set_used(frame, false);
	}
}

/* Резервирует все фреймы, пересекающиеся с полуинтервалом [start, end). */
static void pmm_reserve_region(uintptr_t start, uintptr_t end) {
	uint32_t start_frame;
	uint32_t end_frame;

	if (end <= start) {
		return;
	}

	start_frame = addr_to_frame(start);
	/* Округление вверх: любой байт внутри страницы резервирует всю страницу. */
	end_frame = addr_to_frame(end + PMM_PAGE_SIZE - 1) + 1;
	pmm_reserve_frames(start_frame, end_frame);
}

/* В higher-half ядре: виртуальный адрес → физический (символы linker — виртуальные). */
static uintptr_t virt_to_phys(uintptr_t virt) {
	return virt - KERNEL_VIRTUAL_BASE;
}

/* Номер фрейма сразу после последнего байта региона mmap [addr, addr+len). */
static uint32_t region_end_frame(uint64_t addr, uint64_t len) {
	uint64_t end = addr + len;

	if (end == 0) {
		return 0;
	}

	return addr_to_frame((uintptr_t)(end - 1)) + 1;
}

static uint32_t region_start_frame(uint64_t addr) {
	return addr_to_frame((uintptr_t)addr);
}

/*
 * Свободная RAM по Multiboot: type 1 — available, type 3 — ACPI reclaimable
 * (можно использовать, пока ACPI-таблицы не понадобятся).
 */
static bool pmm_region_usable(uint32_t type) {
	return type == MULTIBOOT_MEMORY_AVAILABLE
		|| type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;
}

static void pmm_mark_highest(uint64_t region_end, uint64_t* highest) {
	if (region_end > *highest) {
		*highest = region_end;
	}
}

/*
 * Строит bitmap физической памяти по карте Multiboot.
 *
 * Алгоритм:
 * 1) Определить верхнюю границу RAM (mmap + mem_upper).
 * 2) Весь bitmap — «занят».
 * 3) Освободить usable-регионы из mmap (или fallback по mem_upper).
 * 4) Зарезервировать ядро, page tables, стек и сам bitmap.
 */
void pmm_init(void) {
	uint64_t highest = 0;
	uint32_t i;
	bool freed_any = false;

	if (!multiboot_is_valid()) {
		return;
	}

	/* Размер PMM = максимальный конец всех регионов mmap. */
	for (i = 0; i < multiboot_mmap_entry_count(); i++) {
		const multiboot_mmap_entry_t* entry = &multiboot_get_mmap()[i];
		pmm_mark_highest(entry->addr + entry->len, &highest);
	}

	/* mem_upper — RAM выше 1 MiB; используем как запасной источник размера. */
	if (multiboot_get_mem_upper_kb() != 0) {
		uint64_t mem_upper_end = 0x00100000ULL + (uint64_t)multiboot_get_mem_upper_kb() * 1024ULL;
		pmm_mark_highest(mem_upper_end, &highest);
	}

	pmm_frame_count = addr_to_frame((uintptr_t)highest) + 1;
	if (pmm_frame_count > PMM_MAX_FRAMES) {
		pmm_frame_count = PMM_MAX_FRAMES;
	}

	if (pmm_frame_count == 0) {
		return;
	}

	/* Пессимистично: пока не знаем карту — всё занято. */
	memset(pmm_bitmap, 0xFF, (pmm_frame_count + 7) / 8);
	pmm_used_count = pmm_frame_count;

	for (i = 0; i < multiboot_mmap_entry_count(); i++) {
		const multiboot_mmap_entry_t* entry = &multiboot_get_mmap()[i];
		uint32_t start_frame = region_start_frame(entry->addr);
		uint32_t end_frame = region_end_frame(entry->addr, entry->len);

		if (pmm_region_usable(entry->type)) {
			pmm_free_frames_range(start_frame, end_frame);
			freed_any = true;
		} else {
			pmm_reserve_frames(start_frame, end_frame);
		}
	}

	/* Fallback: если type в mmap не распознан, используем mem_upper от GRUB. */
	if (!freed_any && multiboot_get_mem_upper_kb() != 0) {
		uint64_t ram_len = (uint64_t)multiboot_get_mem_upper_kb() * 1024ULL;
		uint32_t start_frame = region_start_frame(0x00100000ULL);
		uint32_t end_frame = region_end_frame(0x00100000ULL, ram_len);
		pmm_free_frames_range(start_frame, end_frame);
	}

	/* Физический адрес 0 — IVT и BIOS data; не отдаём аллокатору. */
	pmm_set_used(0, true);

	/* Ядро: _kernel_start в linker = 0x00100000, _kernel_end — виртуальный. */
	pmm_reserve_region(KERNEL_PHYSICAL_START, virt_to_phys((uintptr_t)&_kernel_end));
	pmm_reserve_region(virt_to_phys((uintptr_t)&boot_page_directory),
		virt_to_phys((uintptr_t)&boot_page_table1) + PMM_PAGE_SIZE);
	pmm_reserve_region(virt_to_phys((uintptr_t)&stack_bottom), virt_to_phys((uintptr_t)&stack_top));
	pmm_reserve_region(virt_to_phys((uintptr_t)pmm_bitmap),
		virt_to_phys((uintptr_t)pmm_bitmap) + sizeof(pmm_bitmap));
}

/*
 * Выделяет один свободный физический фрейм (4 KiB).
 * Линейный поиск по bitmap; frame 0 пропускаем (зарезервирован).
 * Возвращает физический адрес или NULL при нехватке памяти.
 */
void* pmm_alloc_frame(void) {
	for (uint32_t frame = 1; frame < pmm_frame_count; frame++) {
		uint32_t byte = frame / 8;
		uint8_t bit = (uint8_t)(1u << (frame % 8));

		if ((pmm_bitmap[byte] & bit) == 0) {
			pmm_set_used(frame, true);
			return (void*)(uintptr_t)(frame * PMM_PAGE_SIZE);
		}
	}

	return 0;
}

/* Возвращает фрейм аллокатору. addr должен быть выровнен на 4096 байт. */
void pmm_free_frame(void* frame) {
	uintptr_t addr = (uintptr_t)frame;

	if (addr == 0 || (addr % PMM_PAGE_SIZE) != 0) {
		return;
	}

	pmm_set_used(addr_to_frame(addr), false);
}

uint32_t pmm_total_frames(void) {
	return pmm_frame_count;
}

uint32_t pmm_used_frames(void) {
	return pmm_used_count;
}

uint32_t pmm_free_frames(void) {
	if (pmm_frame_count < pmm_used_count) {
		return 0;
	}
	return pmm_frame_count - pmm_used_count;
}
