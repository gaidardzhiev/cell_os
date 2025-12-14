/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdint.h>
#include <stdbool.h>
#include "kernel/console.h"
#include "kernel/pf12_arm_irq.h"
#include "arch/arm64/gicv2.h"
#include "arch/arm64/timer.h"

#define ARCH_TIMER_INTID 30u

extern void pf12_install_vectors(void);

static volatile uint32_t tick_count;
static bool timer_ready;
static bool irq_started;
static bool tick_reported;

static inline void enable_irq(void) {
	__asm__ volatile("msr daifclr, #0xf" ::: "memory");
}

static inline uint64_t read_cntpct(void) {
	uint64_t v;
	__asm__ volatile("mrs %0, cntpct_el0" : "=r"(v));
	return v;
}

static inline uint64_t read_cntfrq(void) {
	uint64_t v;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
	return v;
}

static void log_irq_ticks(void) {
	if (tick_reported)
		return;
	tick_reported = true;
	if (tick_count == 0) {
		dbg_puts("#B! irq tick count=00000000\n");
	} else {
		dbg_puts("#B3 irq tick count=");
		dbg_hex32(tick_count);
		dbg_puts("\n");
		arm_timer_disable(); 
	}
}

void pf12_arm_irq_init(void) {
#if CFG_ENABLE_IRQ_TIMER
	pf12_install_vectors();
	gicv2_init();
	timer_ready = arm_timer_init_1ms();
	if (timer_ready) {
		gicv2_enable(ARCH_TIMER_INTID);
	}
#endif
}

static void wait_for_first_tick(void) {
	uint64_t deadline = read_cntpct() + read_cntfrq();
	while (tick_count == 0 && read_cntpct() < deadline) {
		uint32_t iar = gicv2_ack();
		uint32_t intid = iar & 0x3FFu;
		if (intid != 0x3FFu) {
			if (intid == ARCH_TIMER_INTID && timer_ready) {
				tick_count++;
				arm_timer_rearm();
			}
			gicv2_eoi(iar);
		}
		__asm__ volatile("nop");
	}
}

void pf12_arm_irq_start(void){
#if CFG_ENABLE_IRQ_TIMER
	if (timer_ready && !irq_started) {
		irq_started = true;
		tick_count = 0;
		enable_irq();
		arm_timer_rearm();
	}
	if (!tick_reported) {
		wait_for_first_tick();
		log_irq_ticks();
	}
#endif
}

void pf12_arm_irq_report(void) {
#if CFG_ENABLE_IRQ_TIMER
	if (!tick_reported && tick_count == 0) {
		uint64_t deadline = read_cntpct() + (read_cntfrq() / 4u);
		while (tick_count == 0 && read_cntpct() < deadline) {
			uint32_t iar = gicv2_ack();
			uint32_t intid = iar & 0x3FFu;
			if (intid != 0x3FFu) {
				if (intid == ARCH_TIMER_INTID && timer_ready) {
					tick_count++;
					arm_timer_rearm();
				}
				gicv2_eoi(iar);
			}
			__asm__ volatile("nop");
		}
	}
	log_irq_ticks();
#endif
}

void pf12_irq_handler(void) {
#if CFG_ENABLE_IRQ_TIMER
	uint32_t iar = gicv2_ack();
	uint32_t intid = iar & 0x3FFu;
	if (intid == ARCH_TIMER_INTID && timer_ready) {
		tick_count++;
		arm_timer_rearm();
	}
	gicv2_eoi(iar);
#else
	(void)tick_count;
	(void)timer_ready;
#endif
}
