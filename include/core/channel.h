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
#include "core/qos.h"

typedef struct __attribute__((packed)) {
	uint32_t channel_id;
	uint32_t qos;
	uint32_t priority;
	uint64_t bucket;
	uint64_t bucket_max;
	uint64_t refill_per_tick;
} channel_qos_t;

typedef struct {
	uint32_t num_channels;
	const channel_qos_t* qos;
	const uint8_t* adj;
} channel_graph_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(channel_qos_t) == 36, "channel_qos_t ABI drift");
#endif

bool cg_route_allowed(const channel_graph_t* g, uint32_t from, uint32_t to);
void cg_build_full(channel_graph_t* g, channel_qos_t* qos_buf, uint8_t* adj_bs, uint32_t n, uint64_t refill, uint64_t bucket_max);
