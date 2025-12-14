/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/hash.h"
#include "core/update.h"

void updater_init(updater_t* u, genome_storage_t* gs) {
	u->gs = gs;
}

static int free_strand_index(const updater_t* u) {
	return u->gs->preferred ? 0 : 1;
}

static uint8_t* strand_buf(updater_t* u, int idx, size_t* len_out) {
	if (idx==0) {
		if (len_out) *len_out = u->gs->strand_x_len;
		return u->gs->strand_x;
	} else {
		if (len_out) *len_out = u->gs->strand_y_len;
		return u->gs->strand_y;
	}
}

error_code_t update_write_and_verify(updater_t* u, const uint8_t* new_image, size_t new_len, const genome_meta_t* meta_expected) {
	if (!u || !u->gs || !new_image || !meta_expected)
		return E_VERIFY;
	int dst = free_strand_index(u);
	size_t dst_len=0;
	uint8_t* dst_buf = strand_buf(u, dst, &dst_len);
	if (!dst_buf || new_len > dst_len)
		return E_VERIFY;
	memset(dst_buf, 0, dst_len);
	memcpy(dst_buf, new_image, new_len);
	uint8_t idx_h[32], gen_h[32];
	size_t half = new_len/2;
	cell_hash(dst_buf, half, idx_h);
	cell_hash(dst_buf + half, new_len - half, gen_h);
	if (memcmp(idx_h, meta_expected->merkle_idx, 32)!=0)
		return E_VERIFY;
	if (memcmp(gen_h, meta_expected->merkle_genes, 32)!=0)
		return E_VERIFY;
	return E_OK;
}

void update_mark_preferred(updater_t* u, int strand_xy) {
	if (!u || !u->gs)
		return;
	u->gs->preferred = (strand_xy?1:0);
}

void update_rollback(updater_t* u) {
	if (!u || !u->gs)
		return;
	u->gs->preferred ^= 1;
}

int update_preferred(const updater_t* u) {
	return (u && u->gs) ? u->gs->preferred : 0;
}
