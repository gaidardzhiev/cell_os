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
#include "core/events.h"

typedef struct {
	uint32_t debounce_ticks;
	uint32_t batch_max;
	uint32_t rl_refill;
	uint32_t rl_bucket_max;
} irq_policy_t;

typedef struct {
	uint32_t src;
	uint32_t tokens;
	uint32_t last_tick;
} irq_src_state_t;

typedef struct {
	uint32_t tick;
	uint32_t seq;
	irq_policy_t pol;
	irq_src_state_t srcs[64];
	uint32_t nsrc;
	event_frame_t q[128];
	uint32_t q_head, q_len;
	uint32_t dropped;
} irq_bridge_t;

void irq_init(irq_bridge_t* b, const irq_policy_t* pol);

uint32_t irq_register_source(irq_bridge_t* b, uint32_t src_id);

bool irq_raise(irq_bridge_t* b, uint32_t src_idx, event_kind_t kind, const void* payload, uint32_t len);

void irq_poll(irq_bridge_t* b);

bool irq_next(irq_bridge_t* b, event_frame_t* out);
