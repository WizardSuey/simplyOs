# Хранение GDTR (лимит + база)
.section .data
gdtr:
    .word 0      # DW 0 — лимит (2 байта)
    .long 0      # DD 0 — база (4 байта)

# Функция установки GDT
.section .text
.global setGdt
.type setGdt, @function

setGdt:
    # Получаем лимит из первого аргумента (4 байта на стеке, но лимит — 16 бит)
    movw    4(%esp), %ax        # AX = младшие 16 бит первого аргумента (limit)
    movw    %ax, gdtr           # Сохраняем лимит в gdtr (первые 2 байта)

    # Получаем базу из второго аргумента (32 бита)
    movl    8(%esp), %eax       # EAX = второй аргумент (base)
    movl    %eax, gdtr+2        # Сохраняем базу в gdtr (следующие 4 байта)

    # Загружаем GDTR
    lgdt    gdtr                # LGDT [gdtr]

    ret
.size setGdt, . - setGdt