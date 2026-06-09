#include <stdio.h>

#include <kernel/gdt/gdt.h>
#include <kernel/tty.h>  // Для kerror или printf

// Таблица GDT (5 записей: null, kernel code, kernel data, user code, user data)
static GDTEntry gdt_entries[5];
static GDTPtr gdt_ptr;

// Внешняя ассемблерная функция для загрузки GDT
extern void gdt_flush(uint32_t gdt_ptr_addr);

/**
 * Создать дескриптор GDT из базовых параметров
 */
void create_descriptor(uint32_t base, uint32_t limit, uint16_t flag, GDTEntry *entry) {
    // Создаём 64-битный дескриптор (промежуточное представление)
    uint64_t descriptor;
 
    // Старшие 32 бита
    descriptor  =  ((uint64_t)limit & 0x000F0000);  // limit bits 19:16
    descriptor |= ((uint64_t)flag << 8) & 0x00F0FF00; // type, p, dpl, s, g, d/b, l, avl
    descriptor |= ((uint64_t)(base >> 16) & 0x000000FF); // base bits 23:16
    descriptor |= ((uint64_t)base & 0xFF000000);      // base bits 31:24
 
    // Сдвигаем для младшей части
    descriptor <<= 32;
 
    // Младшие 32 бита
    descriptor |= ((uint64_t)base << 16);             // base bits 15:0
    descriptor |= ((uint64_t)limit & 0x0000FFFF);     // limit bits 15:0

    // Разбираем 64-битный дескриптор в структуру GDTEntry
    entry->limit_low    = descriptor & 0xFFFF;
    entry->base_low     = (descriptor >> 16) & 0xFFFF;
    entry->base_middle  = (descriptor >> 32) & 0xFF;
    entry->access       = (descriptor >> 40) & 0xFF;
    entry->granularity  = (descriptor >> 48) & 0xFF;
    entry->base_high    = (descriptor >> 56) & 0xFF;
}

/**
 * Закодировать запись GDT из структуры GDTDescriptor в массив байт
 */
void encodeGdtEntry(uint8_t *target, GDTDescriptor source) {
    // Проверяем, что лимит можно закодировать
    if (source.limit > 0xFFFFF) {
        printf("ERROR: GDT cannot encode limits larger than 0xFFFFF\n");
        return;
    }
    
    // Кодируем лимит
    target[0] = source.limit & 0xFF;
    target[1] = (source.limit >> 8) & 0xFF;
    target[6] = (source.limit >> 16) & 0x0F;
    
    // Кодируем базу
    target[2] = source.base & 0xFF;
    target[3] = (source.base >> 8) & 0xFF;
    target[4] = (source.base >> 16) & 0xFF;
    target[7] = (source.base >> 24) & 0xFF;
    
    // Кодируем access byte (младший байт флагов)
    target[5] = source.flags & 0xFF;
    
    // Кодируем granularity (старший байт флагов)
    target[6] |= ((source.flags >> 8) & 0xF0);
}

/**
 * Инициализация GDT
 */
void gdt_init() {
    // Устанавливаем лимит и базу для GDT
    gdt_ptr.limit = sizeof(GDTEntry) * 5 - 1;
    gdt_ptr.base = (uint32_t)&gdt_entries;
    
    // Нулевой дескриптор (обязателен)
    create_descriptor(0, 0, 0, &gdt_entries[0]);
    
    // Kernel code segment (selector 0x08)
    create_descriptor(0, 0xFFFFFFFF, GDT_CODE_PL0, &gdt_entries[1]);
    
    // Kernel data segment (selector 0x10)
    create_descriptor(0, 0xFFFFFFFF, GDT_DATA_PL0, &gdt_entries[2]);
    
    // User code segment (selector 0x18)
    create_descriptor(0, 0xFFFFFFFF, GDT_CODE_PL3, &gdt_entries[3]);
    
    // User data segment (selector 0x20)
    create_descriptor(0, 0xFFFFFFFF, GDT_DATA_PL3, &gdt_entries[4]);
    
    // Загружаем GDT
    gdt_flush((uint32_t)&gdt_ptr);
}