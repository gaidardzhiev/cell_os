/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#define CWM_MAGIC 0x314D5743u /* CWM1 */
#define CWM_VERSION 1u
#define CWM_QUANT_Q8 1u

typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t version;
	uint16_t header_bytes;
	uint32_t quant;
	uint32_t vocab_size;
	uint32_t context_len;
	uint32_t d_model;
	uint32_t n_layers;
	uint32_t n_heads;
	uint32_t d_ff;
	uint64_t weights_bytes;
	uint64_t total_bytes;
	uint32_t crc32;
	uint32_t reserved;
	uint32_t reserved2;
} cwm_header_t;

_Static_assert(sizeof(cwm_header_t) == 64, "CWM1 header ABI must remain 64 bytes");

typedef struct {
	float scale;
	const int8_t *data;
	uint32_t rows;
	uint32_t cols;
} cwm_q8_matrix_t;

typedef struct {
	const cwm_header_t *h;
	const uint8_t *base;
	const uint8_t *weights;
} cwm_model_t;

int cwm_open(cwm_model_t *m, const void *data, size_t bytes);
uint32_t cwm_crc32(const void *data, size_t bytes);
