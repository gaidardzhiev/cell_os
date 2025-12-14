/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/irq_bridge.h"

void irq_init(irq_bridge_t* b, const irq_policy_t* pol) {
	memset(b, 0, sizeof *b);
	if (pol) b->pol = *pol;
}

uint32_t irq_register_source(irq_bridge_t* b, uint32_t src_id) {
	if (b->nsrc >= 64) return 0xFFFFFFFFu;
	irq_src_state_t* s = &b->srcs[b->nsrc];
	*s = (irq_src_state_t){
		.src = src_id,
		.tokens = b->pol.rl_bucket_max,
		.last_tick = 0
	};
	return b->nsrc++;
}

static inline void q_push(irq_bridge_t* b, const event_frame_t* f) {
	if (b->q_len >= 128) { b->dropped++; return; }
	uint32_t i = (b->q_head + b->q_len) & 127u;
	b->q[i] = *f;
	b->q_len++;
}

bool irq_raise(irq_bridge_t* b, uint32_t src_idx, event_kind_t kind, const void* payload, uint32_t len) {
	if (!b || src_idx >= b->nsrc) return false;
	irq_src_state_t* s = &b->srcs[src_idx];
	if (b->tick - s->last_tick < b->pol.debounce_ticks) return true;
	if (s->tokens == 0) return true;
	s->tokens--;
	event_frame_t f;
	if (!event_pack(&f, ++b->seq, s->src, kind, payload, len)) return false;
	q_push(b, &f);
	s->last_tick = b->tick;
	return true;
}

void irq_poll(irq_bridge_t* b) {
	if (!b) return;
	b->tick++;
	for (uint32_t i=0; i<b->nsrc; i++){
		irq_src_state_t* s = &b->srcs[i];
		uint32_t room = (b->pol.rl_bucket_max > s->tokens) ? (b->pol.rl_bucket_max - s->tokens) : 0;
		uint32_t add  = (b->pol.rl_refill < room) ? b->pol.rl_refill : room;
		s->tokens += add;
	}
}

bool irq_next(irq_bridge_t* b, event_frame_t* out) {
	if (!b || !out || b->q_len == 0) return false;
	*out = b->q[b->q_head];
	b->q_head = (b->q_head + 1) & 127u;
	b->q_len--;
	return true;
}
