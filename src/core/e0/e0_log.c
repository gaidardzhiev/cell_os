/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stddef.h>
#include "core/e0_log.h"
#include "spec/cgf_substrate_e0.h"
#include "spec/cgf_substrate_db.h"
#include "kernel/console.h"

static const char* arch_to_str(uint32_t arch) {
	switch (arch) {
	case CGF_SUBSTRATE_ARCH_X86_64: return "x86_64";
	case CGF_SUBSTRATE_ARCH_ARM64: return "arm64";
	case CGF_SUBSTRATE_ARCH_ARM_M: return "arm_m";
	default: return "UNKNOWN";
	}
}

static const char* tier_to_str(uint32_t tier) {
	switch (tier) {
	case CGF_SUBSTRATE_TIER_PC_BIOS: return "PC_BIOS";
	case CGF_SUBSTRATE_TIER_QEMU_VIRT: return "QEMU_VIRT";
	case CGF_SUBSTRATE_TIER_MCU_CONTROLLER: return "MCU_CONTROLLER";
	default: return "UNKNOWN";
	}
}

static const char* intr_to_str(uint32_t intr) {
	switch (intr) {
	case CGF_INTR_FABRIC_PIC_IOAPIC: return "PIC_IOAPIC";
	case CGF_INTR_FABRIC_LAPIC_ONLY: return "LAPIC_ONLY";
	case CGF_INTR_FABRIC_GICV2: return "GICV2";
	case CGF_INTR_FABRIC_GICV3: return "GICV3";
	case CGF_INTR_FABRIC_ONCHIP_MCU: return "ONCHIP_MCU";
	default: return "UNKNOWN";
	}
}

static const char* timer_to_str(uint32_t tmr) {
	switch (tmr) {
	case CGF_TIMER_FABRIC_PIT: return "PIT";
	case CGF_TIMER_FABRIC_HPET: return "HPET";
	case CGF_TIMER_FABRIC_ARCH_TIMER: return "ARCH_TIMER";
	case CGF_TIMER_FABRIC_ONCHIP_MCU: return "ONCHIP_MCU";
	default: return "UNKNOWN";
	}
}

static void buf_append(char* buf, size_t buf_sz, size_t* off, const char* s) {
	if (!buf || !off || !s || buf_sz == 0) {
		return;
	}
	while (*s && *off + 1 < buf_sz) {
		buf[*off] = *s++;
		(*off)++;
	}
	buf[*off] = '\0';
}

static void buf_append_char(char* buf, size_t buf_sz, size_t* off, char c) {
	if (!buf || !off || *off + 1 >= buf_sz) {
		return;
	}
	buf[*off] = c;
	(*off)++;
	buf[*off] = '\0';
}

static void buf_append_hex32(char* buf, size_t buf_sz, size_t* off, uint32_t v, int width) {
	static const char* hex = "0123456789ABCDEF";
	for (int i = width - 1; i >= 0; --i) {
		uint32_t nib = (v >> (i * 4)) & 0xF;
		buf_append_char(buf, buf_sz, off, hex[nib]);
	}
}

static void buf_append_hex64(char* buf, size_t buf_sz, size_t* off, uint64_t v, int width) {
	static const char* hex = "0123456789ABCDEF";
	for (int i = width - 1; i >= 0; --i) {
		uint64_t nib = (v >> (i * 4)) & 0xF;
		buf_append_char(buf, buf_sz, off, hex[nib]);
	}
}

static void buf_append_u64_dec(char* buf, size_t buf_sz, size_t* off, uint64_t v) {
	char tmp[32];
	int idx = 0;
	if (v == 0) {
		tmp[idx++] = '0';
	}
	while (v > 0 && idx < (int)(sizeof(tmp) - 1)) {
		tmp[idx++] = (char)('0' + (v % 10));
		v /= 10;
	}
	for (int i = idx - 1; i >= 0; --i) {
		buf_append_char(buf, buf_sz, off, tmp[i]);
	}
}

static void console_flags_to_buf(uint32_t console, char* buf, size_t buf_sz) {
	size_t off = 0;
	int first = 1;

	if (console == CGF_CONSOLE_FABRIC_NONE) {
		buf_append(buf, buf_sz, &off, "NONE");
		return;
	}

	if (console & CGF_CONSOLE_FABRIC_COM1) {
		if (!first) buf_append_char(buf, buf_sz, &off, '|');
		first = 0;
		buf_append(buf, buf_sz, &off, "COM1");
	}
	if (console & CGF_CONSOLE_FABRIC_VGA_TEXT) {
		if (!first) buf_append_char(buf, buf_sz, &off, '|');
		first = 0;
		buf_append(buf, buf_sz, &off, "VGA");
	}
	if (console & CGF_CONSOLE_FABRIC_PL011) {
		if (!first) buf_append_char(buf, buf_sz, &off, '|');
		first = 0;
		buf_append(buf, buf_sz, &off, "PL011");
	}
	if (console & CGF_CONSOLE_FABRIC_UART0) {
		if (!first) buf_append_char(buf, buf_sz, &off, '|');
		first = 0;
		buf_append(buf, buf_sz, &off, "UART0");
	}
}

static void features_to_buf(uint32_t feat, char* buf, size_t buf_sz) {
	size_t off = 0;
	int first = 1;
	if (feat == CGF_CPU_FEAT_NONE) {
		buf_append(buf, buf_sz, &off, "none");
		return;
	}
	if (feat & CGF_CPU_FEAT_SSE2) {
		if (!first) buf_append_char(buf, buf_sz, &off, ',');
		first = 0;
		buf_append(buf, buf_sz, &off, "SSE2");
	}
	if (feat & CGF_CPU_FEAT_SSE3) {
		if (!first) buf_append_char(buf, buf_sz, &off, ',');
		first = 0;
		buf_append(buf, buf_sz, &off, "SSE3");
	}
	if (feat & CGF_CPU_FEAT_SSE4_2) {
		if (!first) buf_append_char(buf, buf_sz, &off, ',');
		first = 0;
		buf_append(buf, buf_sz, &off, "SSE4.2");
	}
	if (feat & CGF_CPU_FEAT_AVX) {
		if (!first) buf_append_char(buf, buf_sz, &off, ',');
		first = 0;
		buf_append(buf, buf_sz, &off, "AVX");
	}
	if (feat & CGF_CPU_FEAT_AVX2) {
		if (!first) buf_append_char(buf, buf_sz, &off, ',');
		first = 0;
		buf_append(buf, buf_sz, &off, "AVX2");
	}
	if (feat & CGF_CPU_FEAT_NEON) {
		if (!first) buf_append_char(buf, buf_sz, &off, ',');
		first = 0;
		buf_append(buf, buf_sz, &off, "NEON");
	}
	if (feat & CGF_CPU_FEAT_FP) {
		if (!first) buf_append_char(buf, buf_sz, &off, ',');
		first = 0;
		buf_append(buf, buf_sz, &off, "FP");
	}
	if (feat & CGF_CPU_FEAT_MPU) {
		if (!first) buf_append_char(buf, buf_sz, &off, ',');
		first = 0;
		buf_append(buf, buf_sz, &off, "MPU");
	}
}

void cgf_e0_log_substrate(const handoff_t* h) {
	if (!h) {
		return;
	}
	const cgf_substrate_info_t* s = &h->substrate;
	char buf[256];
	size_t off = 0;
	char console_buf[64] = {0};
	char feat_buf[64] = {0};
	console_flags_to_buf(s->console_fabric, console_buf, sizeof(console_buf));
	features_to_buf(s->cpu_features, feat_buf, sizeof(feat_buf));
	buf_append(buf, sizeof(buf), &off, "#E0 raw code=0x");
	buf_append_hex32(buf, sizeof(buf), &off, s->substrate_code, 8);
	buf_append(buf, sizeof(buf), &off, " arch=");
	buf_append(buf, sizeof(buf), &off, arch_to_str(s->arch));
	buf_append(buf, sizeof(buf), &off, " tier=");
	buf_append(buf, sizeof(buf), &off, tier_to_str(s->tier));
	buf_append(buf, sizeof(buf), &off, " raw=");
	buf_append_hex32(buf, sizeof(buf), &off, s->reserved_u32[0], 8);
	buf_append(buf, sizeof(buf), &off, " feat_mask=");
	buf_append_hex32(buf, sizeof(buf), &off, s->cpu_features, 8);
	buf_append(buf, sizeof(buf), &off, " intr=");
	buf_append(buf, sizeof(buf), &off, intr_to_str(s->intr_fabric));
	buf_append(buf, sizeof(buf), &off, " timer=");
	buf_append(buf, sizeof(buf), &off, timer_to_str(s->timer_fabric));
	buf_append(buf, sizeof(buf), &off, " console=");
	buf_append(buf, sizeof(buf), &off, console_buf);
	buf_append(buf, sizeof(buf), &off, " ram=");
	buf_append_u64_dec(buf, sizeof(buf), &off, s->ram_total_bytes >> 20);
	buf_append(buf, sizeof(buf), &off, "MiB dma32=0x");
	buf_append_hex64(buf, sizeof(buf), &off, s->dma32_boundary, 16);
	buf_append(buf, sizeof(buf), &off, "\n");
	dbg_puts(buf);
	off = 0;
	buf_append(buf, sizeof(buf), &off, "#E0 info \"");
	buf_append(buf, sizeof(buf), &off, arch_to_str(s->arch));
	buf_append(buf, sizeof(buf), &off, " / ");
	buf_append(buf, sizeof(buf), &off, tier_to_str(s->tier));
	buf_append(buf, sizeof(buf), &off, " / ");
	buf_append(buf, sizeof(buf), &off, intr_to_str(s->intr_fabric));
	buf_append(buf, sizeof(buf), &off, " / ");
	buf_append(buf, sizeof(buf), &off, console_buf);
	buf_append(buf, sizeof(buf), &off, " / feats[");
	buf_append(buf, sizeof(buf), &off, feat_buf);
	buf_append(buf, sizeof(buf), &off, "] / ram=");
	buf_append_u64_dec(buf, sizeof(buf), &off, s->ram_total_bytes >> 20);
	buf_append(buf, sizeof(buf), &off, "MiB\"");
	buf_append(buf, sizeof(buf), &off, "\n");
	dbg_puts(buf);
}
