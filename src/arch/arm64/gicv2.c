/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdint.h>
#include "arch/arm64/gicv2.h"
#include "kernel/console.h"

#define GICD_BASE 0x08000000u
#define GICC_BASE 0x08010000u

#define GICD_CTLR       0x000
#define GICD_IGROUPR0   0x080
#define GICD_ISENABLER0 0x100
#define GICD_IPRIORITYR 0x400

#define GICC_CTLR      0x000
#define GICC_PMR       0x004
#define GICC_IAR       0x00C
#define GICC_EOIR      0x010

#define ARCH_TIMER_PPI 30u

static inline void mmio_write32(uint32_t off, uint32_t val) {
	volatile uint32_t* p = (volatile uint32_t*)((uintptr_t)GICD_BASE + off);
	*p = val;
}

static inline uint32_t mmio_read32_cpu(uint32_t off) {
	volatile uint32_t* p = (volatile uint32_t*)((uintptr_t)GICC_BASE + off);
	return *p;
}

static inline void mmio_write32_cpu(uint32_t off, uint32_t val) {
	volatile uint32_t* p = (volatile uint32_t*)((uintptr_t)GICC_BASE + off);
	*p = val;
}

static inline void gicd_set_priority(uint32_t intid, uint8_t prio) {
	volatile uint8_t* p = (volatile uint8_t*)((uintptr_t)GICD_BASE + GICD_IPRIORITYR + intid);
	*p = prio;
}

void gicv2_init(void) {
	mmio_write32(GICD_CTLR, 0);
	mmio_write32_cpu(GICC_CTLR, 0);
	mmio_write32(GICD_IGROUPR0, 0);
	gicd_set_priority(ARCH_TIMER_PPI, 0);
	uint32_t enable = (1u << ARCH_TIMER_PPI);
	mmio_write32(GICD_ISENABLER0, enable);
	mmio_write32_cpu(GICC_PMR, 0xFF);
	mmio_write32_cpu(GICC_CTLR, 0x1);
	mmio_write32(GICD_CTLR, 0x1);
	__asm__ volatile("dsb sy");
	__asm__ volatile("isb");
	dbg_puts("#B1 gicv2 init ok\n");
}

void gicv2_enable(uint32_t intid) {
	if (intid >= 32u)
		return;
	uint32_t bit = 1u << intid;
	mmio_write32(GICD_ISENABLER0, bit);
}

uint32_t gicv2_ack(void) {
	return mmio_read32_cpu(GICC_IAR);
}

void gicv2_eoi(uint32_t raw_iar) {
	mmio_write32_cpu(GICC_EOIR, raw_iar);
}
