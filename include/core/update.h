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
#include <stddef.h>
#include "abi/errors.h"

typedef struct {
	uint8_t *strand_x; size_t strand_x_len;
	uint8_t *strand_y; size_t strand_y_len;
	int preferred;
} genome_storage_t;

typedef struct {
	uint8_t merkle_idx[32];
	uint8_t merkle_genes[32];
	uint8_t trust_root[32];
	uint32_t version_major, version_minor;
} genome_meta_t;

typedef struct {
	genome_storage_t* gs;
} updater_t;

void updater_init(updater_t* u, genome_storage_t* gs);

error_code_t update_write_and_verify(updater_t* u, const uint8_t* new_image, size_t new_len, const genome_meta_t* meta_expected);

void update_mark_preferred(updater_t* u, int strand_xy);

void update_rollback(updater_t* u);

int update_preferred(const updater_t* u);
