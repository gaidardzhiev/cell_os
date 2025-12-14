/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#ifndef CGF_SUBSTRATE_E0_H
#define CGF_SUBSTRATE_E0_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cgf_substrate_arch {
	CGF_SUBSTRATE_ARCH_UNKNOWN = 0,
	CGF_SUBSTRATE_ARCH_X86_64 = 1,
	CGF_SUBSTRATE_ARCH_ARM64 = 2,
	CGF_SUBSTRATE_ARCH_ARM_M = 3,
	
} cgf_substrate_arch_t;


typedef enum cgf_substrate_tier {
	CGF_SUBSTRATE_TIER_UNKNOWN = 0,
	CGF_SUBSTRATE_TIER_PC_BIOS = 1, 
	CGF_SUBSTRATE_TIER_QEMU_VIRT = 2, 
	CGF_SUBSTRATE_TIER_MCU_CONTROLLER = 3, 
	
} cgf_substrate_tier_t;


typedef enum cgf_intr_fabric {
	CGF_INTR_FABRIC_UNKNOWN = 0,
	CGF_INTR_FABRIC_PIC_IOAPIC = 1, 
	CGF_INTR_FABRIC_LAPIC_ONLY = 2,
	CGF_INTR_FABRIC_GICV2 = 3,
	CGF_INTR_FABRIC_GICV3 = 4,
	CGF_INTR_FABRIC_ONCHIP_MCU = 5, 
} cgf_intr_fabric_t;


typedef enum cgf_timer_fabric {
	CGF_TIMER_FABRIC_UNKNOWN = 0,
	CGF_TIMER_FABRIC_PIT = 1, 
	CGF_TIMER_FABRIC_HPET = 2,
	CGF_TIMER_FABRIC_ARCH_TIMER = 3, 
	CGF_TIMER_FABRIC_ONCHIP_MCU = 4, 
} cgf_timer_fabric_t;


typedef enum cgf_console_fabric {
	CGF_CONSOLE_FABRIC_NONE = 0x00000000u,
	CGF_CONSOLE_FABRIC_COM1 = 0x00000001u, 
	CGF_CONSOLE_FABRIC_VGA_TEXT = 0x00000002u, 
	CGF_CONSOLE_FABRIC_PL011 = 0x00000004u, 
	CGF_CONSOLE_FABRIC_UART0 = 0x00000008u, 
	
} cgf_console_fabric_t;


enum {
	CGF_CPU_FEAT_NONE = 0x00000000u,
	CGF_CPU_FEAT_SSE2 = 0x00000001u,
	CGF_CPU_FEAT_SSE3 = 0x00000002u,
	CGF_CPU_FEAT_SSE4_2 = 0x00000004u,
	CGF_CPU_FEAT_AVX = 0x00000008u,
	CGF_CPU_FEAT_AVX2 = 0x00000010u,
	CGF_CPU_FEAT_NEON = 0x00000100u,
	CGF_CPU_FEAT_FP = 0x00000200u,
	CGF_CPU_FEAT_MPU = 0x00010000u, 
};


typedef struct cgf_substrate_info {
	uint32_t substrate_code;	
	uint32_t arch; 
	uint32_t tier; 
	uint32_t intr_fabric; 
	uint32_t timer_fabric; 
	uint32_t console_fabric; 
	uint32_t cpu_features;
	uint32_t reserved_u32[3];
	uint64_t ram_total_bytes; 
	uint64_t dma32_boundary; 
	uint64_t reserved_u64[9];
} cgf_substrate_info_t;


_Static_assert(sizeof(cgf_substrate_info_t) == 128,
	"cgf_substrate_info_t must remain 128 bytes (E0 ABI)");

#ifdef __cplusplus
}
#endif

#endif 
