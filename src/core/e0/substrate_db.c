/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stddef.h>
#include <spec/cgf_substrate_db.h>

static const cgf_substrate_db_entry_t cgf_substrate_db[] = {
	{
		
		.substrate_code = CGF_SUBSTRATE_CODE_X86_PC_GENERIC,
		.arch = CGF_SUBSTRATE_ARCH_X86_64,
		.tier = CGF_SUBSTRATE_TIER_PC_BIOS,
		.intr_fabric = CGF_INTR_FABRIC_PIC_IOAPIC,
		.timer_fabric = CGF_TIMER_FABRIC_PIT,
		.console_fabric = CGF_CONSOLE_FABRIC_COM1 | CGF_CONSOLE_FABRIC_VGA_TEXT,
		.reserved_u32 = { 0, 0, 0 },
	},
	{
		
		.substrate_code = CGF_SUBSTRATE_CODE_ARM64_VIRT_GENERIC,
		.arch = CGF_SUBSTRATE_ARCH_ARM64,
		.tier = CGF_SUBSTRATE_TIER_QEMU_VIRT,
		.intr_fabric = CGF_INTR_FABRIC_GICV2,
		.timer_fabric = CGF_TIMER_FABRIC_ARCH_TIMER,
		.console_fabric = CGF_CONSOLE_FABRIC_PL011,
		.reserved_u32 = { 0, 0, 0 },
	},
};

#define CGF_SUBSTRATE_DB_COUNT (sizeof(cgf_substrate_db) / sizeof(cgf_substrate_db[0]))



const cgf_substrate_db_entry_t* cgf_e0_lookup_x86(uint32_t fm_code) {
	(void)fm_code;
	for (size_t i = 0; i < CGF_SUBSTRATE_DB_COUNT; ++i) {
		if (cgf_substrate_db[i].arch == CGF_SUBSTRATE_ARCH_X86_64) {
			return &cgf_substrate_db[i];
		}
	}
	return NULL;
}

const cgf_substrate_db_entry_t* cgf_e0_lookup_arm64(uint32_t midr_code) {
	(void)midr_code; 
	for (size_t i = 0; i < CGF_SUBSTRATE_DB_COUNT; ++i) {
		if (cgf_substrate_db[i].arch == CGF_SUBSTRATE_ARCH_ARM64) {
			return &cgf_substrate_db[i];
		}
	}
	return NULL;
}

const cgf_substrate_db_entry_t* cgf_e0_lookup(const cgf_e0_probe_raw_t* raw) {
	if (!raw) {
		return NULL;
	}
	switch (raw->arch) {
	case CGF_SUBSTRATE_ARCH_X86_64:
		return cgf_e0_lookup_x86(raw->fm_code);
	case CGF_SUBSTRATE_ARCH_ARM64:
		return cgf_e0_lookup_arm64(raw->midr_code);
	default:
		return NULL;
	}
}
