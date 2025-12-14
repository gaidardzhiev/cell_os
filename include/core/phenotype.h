/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "core/channel.h"

#define PHENO_MAGIC      0x50484E4Fu
#define PHENO_VER_MAJOR  1
#define PHENO_VER_MINOR  0

typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t ver_major;
	uint16_t ver_minor;
	uint16_t arch_id;
	uint16_t reserved;
	uint32_t num_channels;
	uint32_t quantum_ns;
} phenotype_bin_hdr_t;

typedef struct {
	phenotype_bin_hdr_t   hdr;
	const channel_qos_t*  qos;
	const uint8_t*        adj;
} phenotype_bin_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(phenotype_bin_hdr_t) == 20, "phenotype_bin_hdr_t ABI drift");
#endif

bool phenotype_parse(const uint8_t* blob, size_t len, phenotype_bin_t* out);

bool phenotype_to_channel_graph(const phenotype_bin_t* pb, channel_graph_t* out);
