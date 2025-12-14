/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint16_t chan;
	uint16_t sink_id;
	uint16_t max_depth;
	uint16_t rsv;
} cg_entry_t;

typedef struct {
	const cg_entry_t* entries;
	uint32_t count;
} cg_table_t;

enum { CG_SINK_DIAG = 1 };

bool cg_init(const cg_table_t* t);
bool cg_route(uint16_t chan, cg_entry_t* out);
