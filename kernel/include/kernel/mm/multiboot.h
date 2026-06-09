#ifndef KERNEL_MM_MULTIBOOT_H
#define KERNEL_MM_MULTIBOOT_H

#include <stdint.h>
#include <stdbool.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define MULTIBOOT_INFO_MEMORY 0x00000001
#define MULTIBOOT_INFO_MMAP   0x00000040

#define MULTIBOOT_MEMORY_AVAILABLE       1
#define MULTIBOOT_MEMORY_RESERVED        2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3

/* Нормализованная запись mmap (без поля size). */
typedef struct {
	uint64_t addr;
	uint64_t len;
	uint32_t type;
} multiboot_mmap_entry_t;

void multiboot_early_init(uint32_t magic, uint32_t mb_info_phys);

bool multiboot_is_valid(void);
const multiboot_mmap_entry_t* multiboot_get_mmap(void);
uint32_t multiboot_mmap_entry_count(void);

/* mem_upper из multiboot info, в килобайтах (RAM выше 1 MiB). */
uint32_t multiboot_get_mem_upper_kb(void);

#endif
