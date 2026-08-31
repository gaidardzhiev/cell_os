#include <stdint.h>
#include "core/handoff.h"
#include "kernel/console.h"
#include "kernel/cortex_boot.h"

#define E9_PORT 0xE9u
#define COM1_PORT 0x3F8u

static inline void outb(uint16_t port, uint8_t value) {
	__asm__ volatile("outb %0,%1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
	uint8_t v;
	__asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(port));
	return v;
}

void dbg_putchar(char c) {
	outb(E9_PORT, (uint8_t)c);
	while (!(inb((uint16_t)(COM1_PORT + 5u)) & 0x20u)) { }
	outb(COM1_PORT, (uint8_t)c);
}

void dbg_puts(const char *s) {
	while (s && *s) dbg_putchar(*s++);
}

static void put_hex64(uint64_t v) {
	static const char h[]="0123456789abcdef";
	for (int i=60;i>=0;i-=4) dbg_putchar(h[(v >> i) & 15u]);
}

void kernel64_main(const handoff_t *ho) {
	dbg_puts("#E1 Cell OS Cortex kernel\n");
	if (!ho) { dbg_puts("#CORTEX no handoff\n"); for (;;) __asm__ volatile("hlt"); }
	dbg_puts("#CORTEX mem_top=0x"); put_hex64(ho->mem_top); dbg_puts(" ext=0x"); put_hex64(ho->reserved); dbg_puts("\n");
	cortex_boot(ho);
	for (;;) __asm__ volatile("hlt");
}
