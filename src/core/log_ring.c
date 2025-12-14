/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/log_ring.h"

static size_t space_used(const log_ring_t* r) {
	if (r->full) return r->cap;
	if (r->head >= r->tail) return r->head - r->tail;
	return r->cap - (r->tail - r->head);
}

bool log_ring_init(log_ring_t* r, uint8_t* storage, size_t cap) {
	if (!r || !storage || cap < 8) return false;
	r->buf = storage;
	r->cap = cap;
	r->head = 0;
	r->tail = 0;
	r->full = false;
	r->drops = 0;
	return true;
}

static bool has_space(const log_ring_t* r, size_t need) {
	return need <= (r->cap - space_used(r));
}

bool log_ring_enqueue(log_ring_t* r, const uint8_t* data, size_t len) {
	if (!r || !data || len == 0 || len + 4 > r->cap) {
		if (r) r->drops++;
		return false;
	}
	size_t need = len + 4;
	if (!has_space(r, need)) {
		r->drops++;
		return false;
	}
	size_t idx = r->head;
	uint32_t l = (uint32_t)len;
	uint8_t hdr[4] = { (uint8_t)(l & 0xFFu), (uint8_t)((l >> 8) & 0xFFu),
						(uint8_t)((l >> 16) & 0xFFu), (uint8_t)((l >> 24) & 0xFFu)
				};
	for (size_t i = 0; i < 4; ++i) {
		r->buf[idx] = hdr[i];
		idx = (idx + 1) % r->cap;
	}
	for (size_t i = 0; i < len; ++i) {
		r->buf[idx] = data[i];
		idx = (idx + 1) % r->cap;
	}
	r->head = idx;
	if (r->head == r->tail) r->full = true;
	return true;
}

size_t log_ring_drain(log_ring_t* r, int (*writer)(const uint8_t*, size_t)) {
	if (!r || !writer) return 0;
	size_t drained = 0;
	while (space_used(r) > 0) {
		uint8_t hdr[4];
		for (size_t i = 0; i < 4; ++i) {
			hdr[i] = r->buf[r->tail];
			r->tail = (r->tail + 1) % r->cap;
		}
		uint32_t len = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
										((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
		if (len == 0 || len > r->cap) {
			r->drops++;
			r->tail = r->head;
			r->full = false;
			break;
		}
		uint8_t chunk[1024];
		size_t remaining = len;
		while (remaining > 0) {
			size_t part = remaining;
			if (part > sizeof(chunk)) part = sizeof(chunk);
			for (size_t i = 0; i < part; ++i) {
				chunk[i] = r->buf[r->tail];
				r->tail = (r->tail + 1) % r->cap;
			}
			writer(chunk, part);
			remaining -= part;
		}
		r->full = false;
		drained++;
	}
	return drained;
}

uint32_t log_ring_drops(const log_ring_t* r) {
	return r ? r->drops : 0;
}
