/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdint.h>
#include "core/handoff.h"
#include "kernel/console.h"
#include "kernel/proofs.h"
#include <string.h>
#include "core/log_ring.h"
#include "core/kv.h"
#include "core/e0_log.h"
#include "gene/testgene.h"
#if defined(__aarch64__)
#include "kernel/pf12_arm_irq.h"
#endif

static inline void arch_putchar(char c) {
#if defined(__x86_64__)
	asm volatile("outb %0, $0xE9" :: "a"(c));
	uint16_t port = 0x3F8;
	asm volatile("outb %0, %w1" :: "a"(c), "d"(port));
#elif defined(__aarch64__)
	volatile uint32_t* uart = (volatile uint32_t*)0x09000000u;
	while (uart[0x18 / 4] & (1u << 5)) { }
	uart[0x00 / 4] = (uint32_t)(unsigned char)c;
#else
	(void)c;
#endif
}

void dbg_puts(const char* s) {
	while (*s) arch_putchar(*s++);
}

void dbg_hex32(uint32_t v) {
	static const char* hex = "0123456789ABCDEF";
	for (int i = 7; i >= 0; --i){
		arch_putchar(hex[(v >> (i * 4)) & 0xF]);
	}
}

void dbg_hex64(uint64_t v) {
	dbg_hex32((uint32_t)(v >> 32));
	dbg_hex32((uint32_t)(v & 0xFFFFFFFFu));
}

void kernel64_main(const handoff_t* ho) {
	dbg_puts("#E0 KERNEL \"kernel64_main reached\"\n");
	cgf_e0_log_substrate(ho);
	testgene_express(ho);
	dbg_puts("#E1 handoff base=");
	dbg_hex64(ho->phys_base);
	dbg_puts(" len=");
	dbg_hex64(ho->phys_len);
	dbg_puts(" mem=");
	dbg_hex64(ho->mem_top);
	dbg_puts(" cons=");
	dbg_hex64(ho->console_flags);
	dbg_puts("\n");
#if CFG_ENABLE_NONBLOCK_LOG
	static uint8_t ring_buf[4096];
	static log_ring_t ring;
	if (log_ring_init(&ring, ring_buf, sizeof(ring_buf))){
		dbg_puts("#C1 log ring on size=");
		dbg_hex32((uint32_t)sizeof(ring_buf));
		dbg_puts("\n");
	}
#endif
#if CFG_ENABLE_ORG_PKV
	if (kv_init() == KV_OK){
		dbg_puts("#D1 kv init ok\n");
		uint8_t probe[4] = {0xAA,0xBB,0xCC,0xDD};
		(void)kv_put("org", "probe", probe, sizeof(probe));
		size_t n = sizeof(probe);
		uint8_t out[4] = {0};
		if (kv_get("org", "probe", out, &n) == KV_OK && n == sizeof(probe) && memcmp(out, probe, sizeof(probe)) == 0){
			dbg_puts("#D2 kv sanity ok\n");
		}
	}
#endif
#if defined(__aarch64__)
	pf12_arm_irq_init();
	pf12_arm_irq_start();
#endif
	proofs_run_all();
#if defined(__aarch64__)
	pf12_arm_irq_report();
#endif
#if defined(__x86_64__)
	for(;;){
		__asm__ volatile("hlt");
	}
#endif
#if defined(__aarch64__)
	for(;;){
		__asm__ volatile("wfi");
	}
#endif
}
