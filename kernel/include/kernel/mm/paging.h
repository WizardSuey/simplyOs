#ifndef KERNEL_MM_PAGING_H
#define KERNEL_MM_PAGING_H

#include <stdint.h>
#include <stdbool.h>

#define PAGING_PAGE_SIZE 4096

/* Флаги PTE/PDE (младшие 12 бит записи page table). */
#define PAGING_FLAG_PRESENT  0x001  /* Страница/таблица в памяти */
#define PAGING_FLAG_WRITE    0x002  /* Запись разрешена (иначе read-only) */
#define PAGING_FLAG_USER     0x004  /* Доступ из ring 3 (пока не используем) */

void paging_init(void);

bool paging_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags);
void paging_unmap_page(uintptr_t virt);

/* 0, если страница не отображена. */
uintptr_t paging_get_physical(uintptr_t virt);

#endif
