/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/cg.h"

#ifndef CG_MAX_ENTRIES
#define CG_MAX_ENTRIES 16u
#endif

static cg_entry_t g_entries[CG_MAX_ENTRIES];
static uint32_t g_entry_count;

bool cg_init(const cg_table_t* t) {
	if (!t || !t->entries || t->count == 0) {
		g_entry_count = 0;
		return false;
	}
	uint32_t limit = t->count;
	if (limit > CG_MAX_ENTRIES) {
		limit = CG_MAX_ENTRIES;
	}
	for (uint32_t i = 0; i < limit; ++i) {
		g_entries[i] = t->entries[i];
	}
	g_entry_count = limit;
	return true;
}

bool cg_route(uint16_t chan, cg_entry_t* out) {
	if (g_entry_count == 0 || !out) {
		return false;
	}
	for (uint32_t i = 0; i < g_entry_count; ++i) {
		if (g_entries[i].chan == chan) {
			*out = g_entries[i];
			return true;
		}
	}
	return false;
}
