/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "cortex/cwm.h"

uint32_t cwm_crc32(const void *data, size_t bytes) {
	static const uint32_t tab[16] = {
		0x00000000u,0x1DB71064u,0x3B6E20C8u,0x26D930ACu,
		0x76DC4190u,0x6B6B51F4u,0x4DB26158u,0x5005713Cu,
		0xEDB88320u,0xF00F9344u,0xD6D6A3E8u,0xCB61B38Cu,
		0x9B64C2B0u,0x86D3D2D4u,0xA00AE278u,0xBDBDF21Cu
	};
	const uint8_t *p = (const uint8_t *)data;
	uint32_t c = 0xFFFFFFFFu;
	for (size_t i = 0; i < bytes; ++i) {
		c ^= p[i];
		c = (c >> 4) ^ tab[c & 15u];
		c = (c >> 4) ^ tab[c & 15u];
	}
	return ~c;
}

int cwm_open(cwm_model_t *m, const void *data, size_t bytes) {
	if (!m || !data || bytes < sizeof(cwm_header_t)) return 0;
	const cwm_header_t *h = (const cwm_header_t *)data;
	if (h->magic != CWM_MAGIC || h->version != CWM_VERSION) return 0;
	if (h->header_bytes != sizeof(cwm_header_t) || h->quant != CWM_QUANT_Q8) return 0;
	if (!h->vocab_size || h->vocab_size > 256 || !h->context_len || h->context_len > 4096 ||
	    !h->d_model || !h->n_layers || !h->n_heads || !h->d_ff) return 0;
	if (h->d_model % h->n_heads) return 0;
	if (h->total_bytes > bytes || h->total_bytes < h->header_bytes) return 0;
	if (h->weights_bytes != h->total_bytes - h->header_bytes) return 0;
	const uint8_t *payload = (const uint8_t *)data + h->header_bytes;
	if (h->crc32 && cwm_crc32(payload, (size_t)h->weights_bytes) != h->crc32) return 0;
	m->h = h;
	m->base = (const uint8_t *)data;
	m->weights = payload;
	return 1;
}
