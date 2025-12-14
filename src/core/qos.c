/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/qos.h"

typedef struct {
	qos_class_t cls;
	uint32_t max_depth;
	uint32_t depth;
	uint64_t bucket_max;
	uint64_t bucket;
	uint64_t refill;
} qos_chan_t;

#ifndef QOS_MAX_CHANNELS
#define QOS_MAX_CHANNELS 64
#endif

static qos_chan_t g_channels_storage[QOS_MAX_CHANNELS];
static qos_chan_t* g_channels;
static uint32_t g_chan_count;
static uint32_t g_gas;

void qos_init(uint32_t num_channels) {
	if (num_channels > QOS_MAX_CHANNELS) {
		num_channels = QOS_MAX_CHANNELS;
	}
	g_channels = g_channels_storage;
	g_chan_count = num_channels;
	for (uint32_t i = 0; i < g_chan_count; ++i) {
		g_channels[i].cls = QOS_BE;
		g_channels[i].max_depth = 0;
		g_channels[i].depth = 0;
		g_channels[i].bucket_max = 0;
		g_channels[i].bucket = 0;
		g_channels[i].refill = 0;
	}
	g_gas = 0;
}

void qos_cfg_channel(uint16_t chan, qos_class_t cls, uint32_t max_depth, uint64_t bucket_max, uint64_t refill_per_tick) {
	if (!g_channels || chan >= g_chan_count) {
		return;
	}
	qos_chan_t* c = &g_channels[chan];
	c->cls = cls;
	c->max_depth = max_depth;
	c->depth = 0;
	c->bucket_max = bucket_max;
	c->bucket = bucket_max;
	c->refill = refill_per_tick;
}

void qos_set_gas(uint32_t gas) {
	g_gas = gas;
}

uint32_t qos_get_gas(void) {
	return g_gas;
}

qos_status_t qos_try_enqueue(uint16_t chan, uint32_t bytes_cost) {
	if (!g_channels || chan >= g_chan_count) {
		return QOS_DROP;
	}
	qos_chan_t* c = &g_channels[chan];
	if (bytes_cost > g_gas) {
		return QOS_EGAS;
	}
	if (c->max_depth > 0 && c->depth >= c->max_depth) {
		return QOS_BUSY;
	}
	if (bytes_cost > c->bucket) {
		return QOS_DROP;
	}
	g_gas -= bytes_cost;
	c->bucket -= bytes_cost;
	if (c->max_depth > 0) {
		c->depth++;
	}
	return QOS_OK;
}

void qos_tick(void) {
	if (!g_channels) {
		return;
	}
	for (uint32_t i = 0; i < g_chan_count; ++i) {
		qos_chan_t* c = &g_channels[i];
		if (c->bucket < c->bucket_max) {
			uint64_t new_bucket = c->bucket + c->refill;
			c->bucket = (new_bucket > c->bucket_max) ? c->bucket_max : new_bucket;
		}
		if (c->depth > 0) {
			c->depth--;
		}
	}
}
