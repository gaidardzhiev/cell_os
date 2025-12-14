/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#ifndef CGF_SUBSTRATE_DB_H
#define CGF_SUBSTRATE_DB_H

#include <stdint.h>
#include <spec/cgf_substrate_e0.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cgf_substrate_db_entry {
	uint32_t substrate_code;	

	uint32_t arch;		
	uint32_t tier;		
	uint32_t intr_fabric;	
	uint32_t timer_fabric;	
	uint32_t console_fabric;	

	
	uint32_t reserved_u32[3];
} cgf_substrate_db_entry_t;


typedef struct cgf_e0_probe_raw {
	uint32_t arch;	
	uint32_t fm_code;	
	uint32_t midr_code;	
} cgf_e0_probe_raw_t;


const cgf_substrate_db_entry_t* cgf_e0_lookup(const cgf_e0_probe_raw_t* raw);


const cgf_substrate_db_entry_t* cgf_e0_lookup_x86(uint32_t fm_code);
const cgf_substrate_db_entry_t* cgf_e0_lookup_arm64(uint32_t midr_code);


#define CGF_SUBSTRATE_CODE_X86_PC_GENERIC 0x01010001u
#define CGF_SUBSTRATE_CODE_ARM64_VIRT_GENERIC 0x02010001u

#ifdef __cplusplus
}
#endif

#endif 
