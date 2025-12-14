/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>
#include <spec/cgf_substrate_e0.h>

typedef struct {
	uint64_t phys_base;
	uint64_t phys_len;
	uint64_t mem_top;
	uint64_t console_flags;
	uint64_t reserved;
	cgf_substrate_info_t substrate;
} handoff_t;
_Static_assert(sizeof(handoff_t) == 168, "handoff_t must stay 168 bytes");
