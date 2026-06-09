#ifndef KERNEL_MM_PMM_H
#define KERNEL_MM_PMM_H

#include <stdint.h>
#include <stddef.h>

#define PMM_PAGE_SIZE 4096

void pmm_init(void);

void* pmm_alloc_frame(void);
void pmm_free_frame(void* frame);

uint32_t pmm_total_frames(void);
uint32_t pmm_used_frames(void);
uint32_t pmm_free_frames(void);

#endif
