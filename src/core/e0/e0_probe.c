/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/e0.h"
#include "spec/cgf_substrate_e0.h"
#include "spec/cgf_substrate_db.h"
#include "kernel/console.h"

static void cgf_e0_fill_from_db(cgf_substrate_info_t* dst, const cgf_substrate_db_entry_t* src) {
	if (!dst) {
		return;
	}
	memset(dst, 0, sizeof(*dst));
	if (!src) {
		return;
	}
	dst->substrate_code = src->substrate_code;
	dst->arch = src->arch;
	dst->tier = src->tier;
	dst->intr_fabric = src->intr_fabric;
	dst->timer_fabric = src->timer_fabric;
	dst->console_fabric = src->console_fabric;
}

static uint32_t cgf_e0_build_fm_code(uint32_t eax_leaf1) {
	uint32_t stepping = eax_leaf1 & 0xF;
	uint32_t model = (eax_leaf1 >> 4) & 0xF;
	uint32_t family = (eax_leaf1 >> 8) & 0xF;
	uint32_t ext_model = (eax_leaf1 >> 16) & 0xF;
	uint32_t ext_family = (eax_leaf1 >> 20) & 0xFF;
	if (family == 0xF) {
		family += ext_family;
	}
	if (family == 0x6 || family == 0xF) {
		model |= (ext_model << 4);
	}
	return (family << 16) | (model << 8) | stepping;
}

static uint32_t cgf_e0_cpu_features_from_cpuid(uint32_t ecx_leaf1, uint32_t edx_leaf1, uint32_t ebx_leaf7) {
	uint32_t feats = CGF_CPU_FEAT_NONE;
	if (edx_leaf1 & (1u << 26)) {
		feats |= CGF_CPU_FEAT_SSE2;
	}
	if (ecx_leaf1 & (1u << 0)) {
		feats |= CGF_CPU_FEAT_SSE3;
	}
	if (ecx_leaf1 & (1u << 20)) {
		feats |= CGF_CPU_FEAT_SSE4_2;
	}
	if (ecx_leaf1 & (1u << 28)) {
		feats |= CGF_CPU_FEAT_AVX;
	}
	if (ebx_leaf7 & (1u << 5)) {
		feats |= CGF_CPU_FEAT_AVX2;
	}
	return feats;
}

#if defined(__x86_64__)

extern void cgf_cpuid_leaf0(uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);
extern void cgf_cpuid_leaf1(uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);
extern void cgf_cpuid_leaf7(uint32_t* ebx);

void cgf_e0_probe_x86_64(handoff_t* h) {
	if (!h) {
		return;
	}
	uint32_t eax1 = 0, ebx1 = 0, ecx1 = 0, edx1 = 0;
	uint32_t ebx7 = 0;
	cgf_cpuid_leaf1(&eax1, &ebx1, &ecx1, &edx1);
	cgf_cpuid_leaf7(&ebx7);
	cgf_e0_probe_raw_t raw;
	memset(&raw, 0, sizeof(raw));
	raw.arch = CGF_SUBSTRATE_ARCH_X86_64;
	raw.fm_code = cgf_e0_build_fm_code(eax1);
	const cgf_substrate_db_entry_t* entry = cgf_e0_lookup(&raw);
	cgf_substrate_info_t info;
	cgf_e0_fill_from_db(&info, entry);
	info.cpu_features = cgf_e0_cpu_features_from_cpuid(ecx1, edx1, ebx7);
	info.ram_total_bytes = h->mem_top;
	info.dma32_boundary = 0x0000000100000000ULL;
	info.reserved_u32[0] = raw.fm_code;
	h->substrate = info;
}
#endif 

#if defined(__aarch64__)
extern uint64_t cgf_read_midr_el1(void);

void cgf_e0_probe_arm64(handoff_t* h) {
	if (!h) {
		return;
	}
	uint64_t midr = cgf_read_midr_el1();
	cgf_e0_probe_raw_t raw;
	memset(&raw, 0, sizeof(raw));
	raw.arch = CGF_SUBSTRATE_ARCH_ARM64;
	raw.midr_code = (uint32_t)(midr & 0xFFFFFFFFu);
	const cgf_substrate_db_entry_t* entry = cgf_e0_lookup(&raw);
	cgf_substrate_info_t info;
	cgf_e0_fill_from_db(&info, entry);
	info.cpu_features = CGF_CPU_FEAT_NEON | CGF_CPU_FEAT_FP;
	info.ram_total_bytes = h->mem_top;
	info.dma32_boundary = 0;
	info.reserved_u32[0] = raw.midr_code;
	h->substrate = info;
}
#endif 
