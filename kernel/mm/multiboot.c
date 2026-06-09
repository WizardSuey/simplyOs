#include <kernel/mm/multiboot.h>

#define MAX_MMAP_ENTRIES 64
#define IDENTITY_MAP_LIMIT 0x00400000u

#define MULTIBOOT_FLAG_MEMORY 0x00000001
#define MULTIBOOT_FLAG_BOOTDEV 0x00000002
#define MULTIBOOT_FLAG_CMDLINE 0x00000004
#define MULTIBOOT_FLAG_MODS 0x00000008
#define MULTIBOOT_FLAG_AOUT_SYMS 0x00000010
#define MULTIBOOT_FLAG_ELF_SHDR 0x00000020

static bool mb_valid;
static uint32_t mb_mem_upper_kb;
static multiboot_mmap_entry_t mmap_storage[MAX_MMAP_ENTRIES];
static uint32_t mmap_entry_count;

static bool multiboot_ptr_in_identity_map(uint32_t phys) {
	return phys != 0 && phys < IDENTITY_MAP_LIMIT;
}

static uint32_t multiboot_mmap_fields_offset(uint32_t flags) {
	uint32_t offset = sizeof(uint32_t);

	if (flags & MULTIBOOT_FLAG_MEMORY) {
		offset += 8;
	}
	if (flags & MULTIBOOT_FLAG_BOOTDEV) {
		offset += 4;
	}
	if (flags & MULTIBOOT_FLAG_CMDLINE) {
		offset += 4;
	}
	if (flags & MULTIBOOT_FLAG_MODS) {
		offset += 8;
	}
	if (flags & MULTIBOOT_FLAG_AOUT_SYMS) {
		offset += 16;
	}
	if (flags & MULTIBOOT_FLAG_ELF_SHDR) {
		offset += 16;
	}

	return offset;
}

static bool multiboot_parse_mmap_entry(uint32_t entry_phys, multiboot_mmap_entry_t* out) {
	const uint8_t* raw;
	uint32_t size;

	if (!multiboot_ptr_in_identity_map(entry_phys)) {
		return false;
	}

	raw = (const uint8_t*)(uintptr_t)entry_phys;
	size = *(const uint32_t*)raw;

	if (size < 20) {
		return false;
	}

	out->addr = *(const uint64_t*)(raw + 4);
	out->len = *(const uint64_t*)(raw + 12);
	out->type = *(const uint32_t*)(raw + 20);
	return true;
}

void multiboot_early_init(uint32_t magic, uint32_t mb_info_phys) {
	const uint8_t* info_base;
	uint32_t flags;
	uint32_t mmap_offset;
	uint32_t mmap_length;
	uint32_t mmap_addr;
	uint32_t offset;

	mb_valid = false;
	mb_mem_upper_kb = 0;
	mmap_entry_count = 0;

	if (magic != MULTIBOOT_BOOTLOADER_MAGIC || !multiboot_ptr_in_identity_map(mb_info_phys)) {
		return;
	}

	info_base = (const uint8_t*)(uintptr_t)mb_info_phys;
	flags = *(const uint32_t*)info_base;

	if (flags & MULTIBOOT_FLAG_MEMORY) {
		mb_mem_upper_kb = *(const uint32_t*)(info_base + 8);
	}

	if ((flags & MULTIBOOT_INFO_MMAP) == 0) {
		return;
	}

	mmap_offset = multiboot_mmap_fields_offset(flags);
	mmap_length = *(const uint32_t*)(info_base + mmap_offset);
	mmap_addr = *(const uint32_t*)(info_base + mmap_offset + sizeof(uint32_t));

	if (!multiboot_ptr_in_identity_map(mmap_addr)) {
		return;
	}

	offset = 0;
	while (offset < mmap_length && mmap_entry_count < MAX_MMAP_ENTRIES) {
		uint32_t entry_phys = mmap_addr + offset;
		uint32_t entry_size;
		multiboot_mmap_entry_t parsed;

		if (!multiboot_parse_mmap_entry(entry_phys, &parsed)) {
			break;
		}

		mmap_storage[mmap_entry_count++] = parsed;

		entry_size = *(const uint32_t*)(uintptr_t)entry_phys;
		offset += entry_size + sizeof(uint32_t);
	}

	mb_valid = mmap_entry_count > 0;
}

bool multiboot_is_valid(void) {
	return mb_valid;
}

const multiboot_mmap_entry_t* multiboot_get_mmap(void) {
	return mmap_storage;
}

uint32_t multiboot_mmap_entry_count(void) {
	return mmap_entry_count;
}

uint32_t multiboot_get_mem_upper_kb(void) {
	return mb_mem_upper_kb;
}
