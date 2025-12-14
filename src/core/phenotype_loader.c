/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/phenotype.h"

static inline uint32_t bitset_bytes(uint32_t n) {
	uint64_t bits = (uint64_t)n * (uint64_t)n;
	return (uint32_t)((bits + 7ull) >> 3);
}

bool phenotype_parse(const uint8_t* blob, size_t len, phenotype_bin_t* out) {
	if (!blob || !out)
		return false;
	if (len < sizeof(phenotype_bin_hdr_t))
		return false;
	phenotype_bin_hdr_t hdr;
	memcpy(&hdr, blob, sizeof(hdr));
	if (hdr.magic != PHENO_MAGIC)
		return false;
	if (hdr.ver_major != PHENO_VER_MAJOR)
		return false;
	size_t qos_bytes = (size_t)hdr.num_channels * sizeof(channel_qos_t);
	size_t adj_bytes = (size_t)bitset_bytes(hdr.num_channels);
	size_t need = sizeof(hdr) + qos_bytes + adj_bytes;
	if (need != len)
		return false;
	out->hdr = hdr;
	out->qos = (const channel_qos_t*)(blob + sizeof(hdr));
	out->adj = (const uint8_t*)(blob + sizeof(hdr) + qos_bytes);
	return true;
}

bool phenotype_to_channel_graph(const phenotype_bin_t* pb, channel_graph_t* out) {
	if (!pb || !out)
		return false;
	out->num_channels = pb->hdr.num_channels;
	out->qos = pb->qos;
	out->adj = pb->adj;
	return true;
}
