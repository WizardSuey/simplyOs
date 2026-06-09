#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <string.h>

#define KERNEL_VIRTUAL_BASE 0xC0000000u

/*
 * Рекурсивное отображение page directory (recursive paging):
 *
 * PDE[1023] указывает на сам page directory. Тогда «виртуальная» page table
 * для PDE с индексом n лежит по адресу 0xFFC00000 + n * 4096.
 *
 * Зачем: после boot.S identity map снят — новые page tables PMM выдаёт
 * где угодно в RAM, и без рекурсии к ним нельзя обратиться напрямую.
 * См. OSDev wiki: «Page Tables» → «Recursive mapping».
 */
#define RECURSIVE_PD_INDEX 1023u
#define RECURSIVE_PT_BASE  0xFFC00000u

extern char boot_page_directory[];

/* boot_page_directory из boot.S — уже mapped в higher-half. */
static uint32_t* const page_directory = (uint32_t*)boot_page_directory;

static inline uintptr_t virt_to_phys(uintptr_t virt) {
	return virt - KERNEL_VIRTUAL_BASE;
}

/* Виртуальный указатель на page table с индексом pde_idx через рекурсивный слот. */
static inline uint32_t* page_table_virt(uint32_t pde_idx) {
	return (uint32_t*)(RECURSIVE_PT_BASE + (uintptr_t)pde_idx * PAGING_PAGE_SIZE);
}

/* Сброс одной записи TLB после изменения PTE/PDE (иначе CPU может использовать старый mapping). */
static inline void invlpg(uintptr_t virt) {
	__asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

/*
 * Включает рекурсивный слот в boot page directory.
 * Вызывать один раз после pmm_init(), до любых map/unmap.
 */
void paging_init(void) {
	uintptr_t pd_phys = virt_to_phys((uintptr_t)page_directory);

	page_directory[RECURSIVE_PD_INDEX] = (uint32_t)(pd_phys & 0xFFFFF000u)
		| PAGING_FLAG_PRESENT | PAGING_FLAG_WRITE;
}

/*
 * Отображает виртуальную страницу virt на физическую phys.
 *
 * virt разбирается на PDE (bits 31..22) и PTE (bits 21..12).
 * Если PDE пуст — выделяем новую page table через PMM и заполняем нулями.
 * flags — PAGING_FLAG_WRITE / USER; PRESENT добавляется автоматически.
 */
bool paging_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags) {
	uint32_t pde_idx = (uint32_t)(virt >> 22);
	uint32_t pte_idx = (uint32_t)((virt >> 12) & 0x3FF);
	uint32_t* pt;

	if ((phys & (PAGING_PAGE_SIZE - 1)) != 0) {
		return false;
	}

	flags |= PAGING_FLAG_PRESENT;

	if ((page_directory[pde_idx] & PAGING_FLAG_PRESENT) == 0) {
		void* new_pt = pmm_alloc_frame();
		if (new_pt == 0) {
			return false;
		}

		page_directory[pde_idx] = (uint32_t)(uintptr_t)new_pt
			| PAGING_FLAG_PRESENT | PAGING_FLAG_WRITE;

		pt = page_table_virt(pde_idx);
		memset(pt, 0, PAGING_PAGE_SIZE);
	} else {
		pt = page_table_virt(pde_idx);
	}

	/* Младшие 12 бит PTE — флаги; старшие 20 — физ. адрес страницы. */
	pt[pte_idx] = (uint32_t)phys | flags;
	invlpg(virt);
	return true;
}

/* Убирает mapping одной виртуальной страницы. Физический фрейм не освобождает — это задача вызывающего. */
void paging_unmap_page(uintptr_t virt) {
	uint32_t pde_idx = (uint32_t)(virt >> 22);
	uint32_t pte_idx = (uint32_t)((virt >> 12) & 0x3FF);
	uint32_t* pt;

	if ((page_directory[pde_idx] & PAGING_FLAG_PRESENT) == 0) {
		return;
	}

	pt = page_table_virt(pde_idx);
	pt[pte_idx] = 0;
	invlpg(virt);
}

/* Возвращает физический адрес страницы для virt, или 0 если страница не отображена. */
uintptr_t paging_get_physical(uintptr_t virt) {
	uint32_t pde_idx = (uint32_t)(virt >> 22);
	uint32_t pte_idx = (uint32_t)((virt >> 12) & 0x3FF);
	uint32_t* pt;
	uint32_t entry;

	if ((page_directory[pde_idx] & PAGING_FLAG_PRESENT) == 0) {
		return 0;
	}

	pt = page_table_virt(pde_idx);
	entry = pt[pte_idx];

	if ((entry & PAGING_FLAG_PRESENT) == 0) {
		return 0;
	}

	return (uintptr_t)(entry & 0xFFFFF000u);
}
