#ifndef KERNEL_MM_KHEAP_H
#define KERNEL_MM_KHEAP_H

#include <stddef.h>
#include <stdint.h>

/* Инициализация кучи ядра (после paging_init). */
void kheap_init(void);

/* Динамическое выделение / освобождение памяти в higher-half. */
void* kmalloc(size_t size);
void kfree(void* ptr);

/* Статистика: отmapped виртуальный размер и сумма занятых блоков. */
size_t kheap_total_bytes(void);
size_t kheap_used_bytes(void);

#endif
