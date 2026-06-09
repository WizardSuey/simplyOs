#include <kernel/mm/kheap.h>
#include <kernel/mm/paging.h>
#include <kernel/mm/pmm.h>
#include <stdbool.h>
#include <string.h>

/*
 * Виртуальный диапазон кучи — выше boot map (0xC03FFFFF) и paging-теста (0xC1000000).
 * Страницы подключаются по мере необходимости через paging_map_page + PMM.
 */
#define KHEAP_START     0xC2000000u
#define KHEAP_MAX_BYTES (16u * 1024u * 1024u)

/* Выравнивание указателей kmalloc и минимальный размер блока (заголовок + данные). */
#define KHEAP_ALIGN      8u
#define KHEAP_MIN_BLOCK  (sizeof(kheap_block_t) + KHEAP_ALIGN)
/* При расширении кучи map минимум столько страниц за один вызов. */
#define KHEAP_GROW_PAGES 4u

/*
 * Заголовок блока кучи. Все блоки (свободные и занятые) образуют
 * двусвязный список в порядке возрастания адресов — это нужно для coalesce.
 *
 * Layout в памяти:
 *   [ kheap_block_t | user data... ]
 *   ^               ^
 *   block           указатель, который возвращает kmalloc
 */
typedef struct kheap_block {
	size_t size;                  /* Размер всего блока: заголовок + полезная область */
	bool is_free;
	struct kheap_block* next;
	struct kheap_block* prev;
} kheap_block_t;

static uintptr_t kheap_start = KHEAP_START;
static uintptr_t kheap_end = KHEAP_START;   /* Первый ещё не отmapped байт кучи */
static kheap_block_t* kheap_first;          /* Голова списка блоков */

static size_t kheap_align(size_t n) {
	return (n + KHEAP_ALIGN - 1) & ~(KHEAP_ALIGN - 1);
}

/* По пользовательскому указателю находим заголовок блока (он лежит прямо перед данными). */
static kheap_block_t* kheap_block_from_ptr(void* ptr) {
	return (kheap_block_t*)((uint8_t*)ptr - sizeof(kheap_block_t));
}

/*
 * Расширяет кучу: map новых страниц начиная с kheap_end и добавляет
 * свободный блок в конец списка (или сливает с последним, если он свободный).
 */
static bool kheap_extend(size_t min_bytes) {
	size_t mapped = kheap_end - kheap_start;
	size_t need_bytes;
	size_t pages;
	uintptr_t map_at;
	kheap_block_t* block;
	kheap_block_t* last;

	if (mapped >= KHEAP_MAX_BYTES) {
		return false;
	}

	need_bytes = min_bytes;
	if (need_bytes < KHEAP_GROW_PAGES * PAGING_PAGE_SIZE) {
		need_bytes = KHEAP_GROW_PAGES * PAGING_PAGE_SIZE;
	}
	pages = (need_bytes + PAGING_PAGE_SIZE - 1) / PAGING_PAGE_SIZE;

	map_at = kheap_end;
	for (size_t i = 0; i < pages; i++) {
		void* frame;

		if (map_at + PAGING_PAGE_SIZE - kheap_start > KHEAP_MAX_BYTES) {
			return false;
		}

		frame = pmm_alloc_frame();
		if (frame == 0) {
			return false;
		}

		if (!paging_map_page(map_at, (uintptr_t)frame, PAGING_FLAG_WRITE)) {
			pmm_free_frame(frame);
			return false;
		}

		map_at += PAGING_PAGE_SIZE;
	}

	/* Новый свободный блок покрывает только что отmapped диапазон. */
	block = (kheap_block_t*)kheap_end;
	block->size = map_at - kheap_end;
	block->is_free = true;
	block->next = NULL;
	block->prev = NULL;

	if (kheap_first == NULL) {
		kheap_first = block;
	} else {
		last = kheap_first;
		while (last->next != NULL) {
			last = last->next;
		}

		/* Новая память сразу после last — можно слить, если last свободен. */
		if (last->is_free && (uintptr_t)last + last->size == (uintptr_t)block) {
			last->size += block->size;
			kheap_end = map_at;
			return true;
		}

		block->prev = last;
		last->next = block;
	}

	kheap_end = map_at;
	return true;
}

/*
 * Делит блок при выделении: needed байт — занятый блок, остаток — новый свободный.
 * Если остаток меньше KHEAP_MIN_BLOCK, split не делаем — отдаём весь блок целиком.
 */
static void kheap_split(kheap_block_t* block, size_t needed) {
	kheap_block_t* remainder;

	if (block->size < needed + KHEAP_MIN_BLOCK) {
		block->is_free = false;
		return;
	}

	remainder = (kheap_block_t*)((uint8_t*)block + needed);
	remainder->size = block->size - needed;
	remainder->is_free = true;
	remainder->next = block->next;
	remainder->prev = block;

	if (block->next != NULL) {
		block->next->prev = remainder;
	}

	block->size = needed;
	block->is_free = false;
	block->next = remainder;
}

/* Сливает block с соседними свободными блоками (next, затем prev). */
static void kheap_coalesce(kheap_block_t* block) {
	kheap_block_t* next;
	kheap_block_t* prev;

	next = block->next;
	if (next != NULL && next->is_free) {
		block->size += next->size;
		block->next = next->next;
		if (next->next != NULL) {
			next->next->prev = block;
		}
	}

	prev = block->prev;
	if (prev != NULL && prev->is_free) {
		prev->size += block->size;
		prev->next = block->next;
		if (block->next != NULL) {
			block->next->prev = prev;
		}
	}
}

/*
 * Инициализация кучи. Вызывать после paging_init().
 * Map первую порцию страниц и создаёт один большой свободный блок.
 */
void kheap_init(void) {
	kheap_start = KHEAP_START;
	kheap_end = KHEAP_START;
	kheap_first = NULL;

	if (!kheap_extend(PAGING_PAGE_SIZE)) {
		return;
	}
}

/*
 * Выделяет size байт из кучи (first-fit).
 *
 * 1) Ищем первый свободный блок достаточного размера.
 * 2) Если не нашли — расширяем кучу (kheap_extend) и повторяем.
 * 3) Возвращаем указатель на данные (сразу после заголовка) или NULL.
 */
void* kmalloc(size_t size) {
	size_t needed;
	kheap_block_t* cur;

	if (size == 0) {
		return NULL;
	}

	needed = kheap_align(size + sizeof(kheap_block_t));
	if (needed < KHEAP_MIN_BLOCK) {
		needed = KHEAP_MIN_BLOCK;
	}

	for (;;) {
		for (cur = kheap_first; cur != NULL; cur = cur->next) {
			if (cur->is_free && cur->size >= needed) {
				kheap_split(cur, needed);
				return (void*)((uint8_t*)cur + sizeof(kheap_block_t));
			}
		}

		if (!kheap_extend(needed)) {
			return NULL;
		}
	}
}

/* Помечает блок свободным и сливает с соседями. NULL безопасен (как free в libc). */
void kfree(void* ptr) {
	kheap_block_t* block;

	if (ptr == NULL) {
		return;
	}

	block = kheap_block_from_ptr(ptr);
	block->is_free = true;
	kheap_coalesce(block);
}

/* Сколько виртуальной памяти кучи уже отmapped (не равно физ. RAM — страницы через PMM). */
size_t kheap_total_bytes(void) {
	return kheap_end - kheap_start;
}

/* Сумма size всех занятых блоков (включая их заголовки). */
size_t kheap_used_bytes(void) {
	size_t used = 0;
	kheap_block_t* cur;

	for (cur = kheap_first; cur != NULL; cur = cur->next) {
		if (!cur->is_free) {
			used += cur->size;
		}
	}

	return used;
}
