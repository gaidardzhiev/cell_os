/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>

#define NCI_ABI_MAJOR 1
#define NCI_ABI_MINOR 0

typedef struct __attribute__((packed)) {
	uint64_t cgf_base, cgf_len;
	uint64_t idx_off, idx_len;
	uint8_t  trust_root[32];
	uint8_t  merkle_idx[32];
	uint8_t  merkle_genes[32];
	uint64_t phenotype_off, phenotype_len;
	uint32_t ch0_id;
	uint32_t arch_id;
} handoff_t;
