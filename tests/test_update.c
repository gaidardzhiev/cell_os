/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/hash.h"
#include "core/update.h"
#include "tools/mock_storage.h"

int main(void) {
	genome_storage_t gs;
	mock_storage_alloc(&gs, 4096);
	updater_t U; updater_init(&U, &gs);
	uint8_t img[2048];
	for (int i=0;i<2048;i++) img[i]=(uint8_t)i;
	genome_meta_t meta = {0};
	cell_hash(img, 1024, meta.merkle_idx);
	cell_hash(img + 1024, 1024, meta.merkle_genes);
	assert(update_write_and_verify(&U, img, sizeof(img), &meta) == E_OK);
	int free_idx = (update_preferred(&U)?0:1);
	update_mark_preferred(&U, free_idx);
	assert(update_preferred(&U) == free_idx);
	update_rollback(&U);
	assert(update_preferred(&U) == (free_idx ^ 1));
	mock_storage_free(&gs);
	puts("#E7 update ok");
	return 0;
}
