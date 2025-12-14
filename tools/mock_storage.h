/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "core/update.h"

static inline void mock_storage_alloc(genome_storage_t* gs, size_t cap) {
	gs->strand_x = (uint8_t*)malloc(cap);
	gs->strand_y = (uint8_t*)malloc(cap);
	gs->strand_x_len = gs->strand_y_len = cap;
	gs->preferred = 0;
	memset(gs->strand_x, 0, cap);
	memset(gs->strand_y, 0, cap);
}

static inline void mock_storage_free(genome_storage_t* gs) {
	free(gs->strand_x);
	free(gs->strand_y);
	memset(gs, 0, sizeof *gs);
}
