/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/channel.h"

static inline bool cg_bounds_ok(const channel_graph_t* g, uint32_t idx) {
	return g && g->adj && g->qos && idx < g->num_channels;
}

bool cg_route_allowed(const channel_graph_t* g, uint32_t from, uint32_t to) {
	if (!cg_bounds_ok(g, from) || !cg_bounds_ok(g, to)) {
		return false;
	}
	uint32_t n = g->num_channels;
	uint32_t bit = from * n + to;
	uint32_t byte = bit >> 3;
	uint8_t mask = (uint8_t)(1u << (bit & 7u));
	return (g->adj[byte] & mask) != 0;
}

void cg_build_full(channel_graph_t* g, channel_qos_t* qos_buf, uint8_t* adj_bs, uint32_t n, uint64_t refill, uint64_t bucket_max) {
	if (!g || !qos_buf || !adj_bs) {
		return;
	}
	g->num_channels = n;
	g->qos = qos_buf;
	g->adj = adj_bs;
	for (uint32_t i = 0; i < n; ++i) {
		channel_qos_t* q = &qos_buf[i];
		memset(q, 0, sizeof(*q));
		q->channel_id = i;
		q->qos = QOS_BE;
		q->priority = (uint32_t)(i & 7u);
		q->bucket = bucket_max;
		q->bucket_max = bucket_max;
		q->refill_per_tick = refill;
	}
	uint32_t bits = n * n;
	uint32_t bytes = (bits + 7u) / 8u;
	memset(adj_bs, 0, bytes);
	for (uint32_t from = 0; from < n; ++from) {
		for (uint32_t to = 0; to < n; ++to) {
			uint32_t bit = from * n + to;
			adj_bs[bit >> 3] |= (uint8_t)(1u << (bit & 7u));
		}
	}
}
