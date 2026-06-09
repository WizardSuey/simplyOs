# Declare constants for the multiboot header.
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Объявление заголовока мультизагрузки, который помечает программу как ядро.
.section .multiboot.data, "aw"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Выделим начальный стек.
.section .bootstrap_stack, "aw", @nobits
.global stack_bottom
stack_bottom:
.skip 16384 # 16 KiB
.global stack_top
stack_top:

# Предварительно выделите страницы, используемые для подкачки. Не указывайте адреса в жестком коде и предполагайте, что они
# доступны, поскольку загрузчик мог загрузить туда свои многозагрузочные структуры или
# модули. Это позволяет загрузчику понять, что он должен избегать использования адресов.
.section .bss, "aw", @nobits
	.align 4096
.global boot_page_directory
boot_page_directory:
	.skip 4096
.global boot_page_table1
boot_page_table1:
	.skip 4096

.global multiboot_saved_magic
.global multiboot_saved_info
multiboot_saved_magic:
	.long 0
multiboot_saved_info:
	.long 0
# Могут потребоваться дополнительные таблицы страниц, если размер ядра превысит 3 МБ.

# Точка входа в ядро.
.extern multiboot_early_init
.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
	# Физический адрес boot_page_table1.
	movl $(boot_page_table1 - 0xC0000000), %edi
	# Первый адрес, который отображается на карте, - это адрес 0.
	movl $0, %esi
	# Отобразите 1023 страницы. 1024-й будет текстовый буфер VGA.
	movl $1023, %ecx
1:
	# Identity-map каждую страницу в этой таблице (нужно для multiboot info/mmap в low memory).
	movl %esi, %edx
	orl $0x003, %edx
	movl %edx, (%edi)
2:
	# Размер страницы - 4096 байт
	addl $4096, %esi
	# Размер записей в boot_page_table1 равен 4 байтам.
	addl $4, %edi
	# Переходим к следующей записи, если мы еще не закончили.
	loop 1b
3:
	# Преобразуйте видеопамять VGA в 0xC03FF000 как "существующую, доступную для записи".
	movl $(0x000B8000 | 0x003), boot_page_table1 - 0xC0000000 + 1023 * 4

	# Таблица страниц используется как для записи каталога страниц 0 (фактически от 0x0
	# до 0x3FFFFF) (таким образом, идентификация сопоставляется ядру), так и для записи каталога страниц
	# 768 (фактически от 0xC0000000 до 0xC03FFFFF) (таким образом, отображая его верхней половине). 
	# Ядро идентифицируется, поскольку включение подкачки приводит к
	# не изменять следующую инструкцию, которая по-прежнему является физической. Процессор
	# вместо этого выдал бы ошибку страницы, если бы не было сопоставления идентификаторов.

	# Сопоставьте таблицу страниц с обоими виртуальными адресами 0x00000000 и 0xc00000000.
	movl $(boot_page_table1 - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 0
	movl $(boot_page_table1 - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 768 * 4

	# Установите для параметра cr3 значение адреса каталога boot_page_directory.
	movl $(boot_page_directory - 0xC0000000), %ecx
	movl %ecx, %cr3

	# Включите подкачку по страницам и защиту от записи.
	movl %cr0, %ecx
	orl $0x80010000, %ecx
	movl %ecx, %cr0

	# Прыгайте на более высокую половину с помощью абсолютного прыжка.
	lea 4f, %ecx
	jmp *%ecx

.section .text

4:
	# На этом этапе paging включён; identity map (PDE 0) ещё активен.

	# Настройте стек.
	mov $stack_top, %esp

	# Сохраняем регистры GRUB до вызова C-кода.
	movl %eax, multiboot_saved_magic
	movl %ebx, multiboot_saved_info

	call _init

	# Копируем multiboot mmap, пока identity map ещё активен.
	pushl multiboot_saved_info
	pushl multiboot_saved_magic
	call multiboot_early_init
	addl $8, %esp

	# Отмените identity map — low memory больше не нужна.
	movl $0, boot_page_directory + 0

	# Перезагрузите cr3, чтобы принудительно сбросить TLB.
	movl %cr3, %ecx
	movl %ecx, %cr3

	# Enter the high-level kernel.
	call kernel_main


	# Infinite loop if the system has nothing more to do.
	cli
1:	hlt
	jmp 1b
