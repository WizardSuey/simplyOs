#ifndef KERNEL_IO_IO_H
#define KERNEL_IO_IO_H

#include <stdint.h>

/* Чтение байта из I/O-порта (инструкция IN). */
static inline uint8_t inb(uint16_t port) {
	uint8_t ret;
	__asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

/* Запись байта в I/O-порт (инструкция OUT). */
static inline void outb(uint16_t port, uint8_t val) {
	__asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * Короткая пауза для старых устройств.
 * Запись в порт 0x80 (диагностика POST) даёт CPU время обработать команду PIC.
 */
static inline void io_wait(void) {
	outb(0x80, 0);
}

#endif
