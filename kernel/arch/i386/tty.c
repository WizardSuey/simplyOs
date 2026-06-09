#include <stdbool.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/io/io.h>

#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const size_t TAB_WIDTH = 8;

/* Регистры CRT-контроллера VGA для аппаратного курсора. */
static const uint16_t VGA_CRTC_INDEX = 0x3D4;
static const uint16_t VGA_CRTC_DATA  = 0x3D5;

static uint16_t* const VGA_MEMORY = (uint16_t*) 0xC03FF000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

/* Перемещает мигающий аппаратный курсор VGA на (row, column). */
static void terminal_sync_cursor(void) {
	uint16_t pos = (uint16_t)(terminal_row * VGA_WIDTH + terminal_column);

	outb(VGA_CRTC_INDEX, 0x0F);
	outb(VGA_CRTC_DATA, (uint8_t)(pos & 0xFF));
	outb(VGA_CRTC_INDEX, 0x0E);
	outb(VGA_CRTC_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

/*
 * Включает аппаратный курсор (мигание — встроено в VGA).
 * scanline 0..15: блок на всю высоту символа.
 */
static void terminal_enable_hw_cursor(void) {
	outb(VGA_CRTC_INDEX, 0x0A);
	outb(VGA_CRTC_DATA, (inb(VGA_CRTC_DATA) & 0xC0) | 0);
	outb(VGA_CRTC_INDEX, 0x0B);
	outb(VGA_CRTC_DATA, (inb(VGA_CRTC_DATA) & 0xE0) | 15);
}

static void terminal_scroll(void) {
	for (size_t y = 1; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t dst = (y - 1) * VGA_WIDTH + x;
			const size_t src = y * VGA_WIDTH + x;
			terminal_buffer[dst] = terminal_buffer[src];
		}
	}
	for (size_t x = 0; x < VGA_WIDTH; x++) {
		const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
		terminal_buffer[index] = vga_entry(' ', terminal_color);
	}
	if (terminal_row > 0) {
		terminal_row--;
	}
}

static void terminal_newline(void) {
	terminal_column = 0;
	if (++terminal_row >= VGA_HEIGHT) {
		terminal_scroll();
	}
}

void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
	terminal_enable_hw_cursor();
	terminal_sync_cursor();
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
	unsigned char uc = (unsigned char)c;

	if (uc == '\n') {
		terminal_newline();
		terminal_sync_cursor();
		return;
	}

	if (uc == '\r') {
		terminal_column = 0;
		terminal_sync_cursor();
		return;
	}

	if (uc == '\b') {
		if (terminal_column > 0) {
			terminal_column--;
			terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
		}
		terminal_sync_cursor();
		return;
	}

	if (uc == '\t') {
		while (terminal_column < VGA_WIDTH && (terminal_column % TAB_WIDTH) != 0) {
			terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
			terminal_column++;
		}
		terminal_sync_cursor();
		return;
	}

	if (uc == 27) {
		return;
	}

	terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_newline();
	}
	terminal_sync_cursor();
}

void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++) {
		terminal_putchar(data[i]);
	}
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}
