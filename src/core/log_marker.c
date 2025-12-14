/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/log_marker.h"

#ifdef __x86_64__
static inline void out8(uint16_t port, uint8_t val) {
	__asm__ volatile ("outb %0,%1" :: "a"(val), "Nd"(port));
}

void log_marker_port_e9(const char* s) {
	for (; *s; ++s) out8(0xE9, (uint8_t)*s);
}
#endif

#ifdef __aarch64__
static inline void mmio_write32(uintptr_t addr, uint32_t val) {
	*(volatile uint32_t*)addr = val;
}

static inline uint32_t mmio_read32(uintptr_t addr) {
	return *(volatile uint32_t*)addr;
}

#define PL011_UARTDR_OFF 0x00u
#define PL011_UARTFR_OFF 0x18u
#define PL011_FR_TXFF    (1u << 5)
#define PL011_FR_BUSY    (1u << 3)

void log_marker_pl011(const char* s) {
	for (; *s; ++s){
		while (mmio_read32((uintptr_t)PL011_BASE + PL011_UARTFR_OFF) & PL011_FR_TXFF) { }
		mmio_write32((uintptr_t)PL011_BASE + PL011_UARTDR_OFF, (uint32_t)(unsigned char)(*s));
	}
	while (mmio_read32((uintptr_t)PL011_BASE + PL011_UARTFR_OFF) & PL011_FR_BUSY) { }
}
#endif
