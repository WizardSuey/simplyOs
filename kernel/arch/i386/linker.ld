/* The bootloader will look at this image and start execution at the symbol
   designated at the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	. = 0x00100000;
	/* Объем ядра будет составлять 3 ГБ + 1 МБ в виртуальном адресном пространстве, */
	/*, которое будет сопоставлено с 1 МБ в физическом адресном пространстве. */
	/* Обратите внимание, что мы выравниваем разделы по страницам. */

	_kernel_start = .;
	.multiboot.data : {
		*(.multiboot.data)
	}

	.multiboot.text : {
		*(.multiboot.text)
	}

	. += 0xC0000000;
	/* Добавьте символ, указывающий на начальный адрес ядра. */
	/* First put the multiboot header, as it is required to be put very early
	   early in the image or the bootloader won't recognize the file format.
	   Next we'll put the .text section. */
	.text ALIGN (4K) : AT (ADDR (.text) - 0xC0000000)
	{
		*(.text)
	}

	/* Read-only data. */
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000)
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000)
	{
		*(.data)
	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}

	/* Добавьте символ, указывающий конечный адрес ядра. */
	_kernel_end = .;
}
