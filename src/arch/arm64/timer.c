/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdint.h>
#include <stdbool.h>
#include "arch/arm64/timer.h"
#include "kernel/console.h"

#define ARCH_TIMER_CTL_ENABLE (1u << 0)
#define ARCH_TIMER_CTL_IMASK  (1u << 1)

static inline uint64_t cntfrq(void) {
	uint64_t v;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
	return v;
}

static inline void cntp_tval_write(uint32_t v) {
	__asm__ volatile("msr cntp_tval_el0, %0" :: "r"(v));
}

static inline void cntp_ctl_write(uint32_t v) {
	__asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(v));
}

bool arm_timer_init_1ms(void) {
	uint64_t freq = cntfrq();
	uint32_t tval = (uint32_t)(freq / 1000u);
	if (tval == 0) {
		return false;
	}
	cntp_tval_write(tval);
	cntp_ctl_write(ARCH_TIMER_CTL_ENABLE);
	__asm__ volatile("isb");
	dbg_puts("#B2 arch timer 1ms ok\n");
	return true;
}

void arm_timer_rearm(void) {
	uint64_t freq = cntfrq();
	uint32_t tval = (uint32_t)(freq / 1000u);
	if (tval == 0) tval = 1;
	cntp_tval_write(tval);
	cntp_ctl_write(ARCH_TIMER_CTL_ENABLE);
}

void arm_timer_disable(void) {
	cntp_ctl_write(0);
}
