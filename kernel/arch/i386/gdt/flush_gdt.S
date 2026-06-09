.section .text
.global gdt_flush
.type gdt_flush, @function

gdt_flush:
    # Загружаем новый GDT
    movl 4(%esp), %eax    # Получаем адрес GDTPtr
    lgdt (%eax)           # Загружаем GDT
    
    # Обновляем сегменты
    movw $0x10, %ax       # Селектор данных ядра (0x10)
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    
    # Дальний переход для обновления CS (0x08 - код ядра)
    ljmp $0x08, $.Lflush
.Lflush:
    ret
.size gdt_flush, . - gdt_flush