.set KERNEL_DATA_SEG, 0x10

.macro irq_stub num
.global irq_stub_\num
.type irq_stub_\num, @function
.align 16
irq_stub_\num:
    pushl $0
    pushl $\num
    jmp irq_common_stub
.endm

.extern irq_handler

irq_stub 0
irq_stub 1
irq_stub 2
irq_stub 3
irq_stub 4
irq_stub 5
irq_stub 6
irq_stub 7
irq_stub 8
irq_stub 9
irq_stub 10
irq_stub 11
irq_stub 12
irq_stub 13
irq_stub 14
irq_stub 15

.type irq_common_stub, @function
irq_common_stub:
    pusha

    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs

    movw $KERNEL_DATA_SEG, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    pushl %esp
    call irq_handler
    addl $4, %esp

    popl %gs
    popl %fs
    popl %es
    popl %ds

    popa
    addl $8, %esp
    iret

.section .rodata
.global irq_stub_table
irq_stub_table:
    .long irq_stub_0
    .long irq_stub_1
    .long irq_stub_2
    .long irq_stub_3
    .long irq_stub_4
    .long irq_stub_5
    .long irq_stub_6
    .long irq_stub_7
    .long irq_stub_8
    .long irq_stub_9
    .long irq_stub_10
    .long irq_stub_11
    .long irq_stub_12
    .long irq_stub_13
    .long irq_stub_14
    .long irq_stub_15
