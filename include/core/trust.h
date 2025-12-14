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
#include <string.h>

typedef struct {
	uint8_t  active_key_id;
	uint8_t  keys[4][32];
	uint8_t  slots;
	uint32_t version;
} trust_set_t;

static inline void trust_init(trust_set_t* t, const uint8_t (*raw)[32], uint8_t N, uint8_t active_id, uint32_t version) {
	memset(t,0,sizeof *t);
	if (N>4) N=4;
	t->slots=N; t->active_key_id=active_id; t->version=version;
	for (uint8_t i=0;i<N;i++) memcpy(t->keys[i], raw[i], 32);
}

static inline void trust_rotate(trust_set_t* t, uint8_t new_active_id) {
	if (!t)
		return;
	t->active_key_id = (t->slots? (new_active_id % t->slots) : 0);
	t->version++;
}

static inline int trust_active_mac(const trust_set_t* t, uint8_t* key_id_out, uint8_t raw32_out[32]) {
	if (!t || !t->slots)
		return 0;
	if (key_id_out) *key_id_out = t->active_key_id;
	if (raw32_out)  memcpy(raw32_out, t->keys[t->active_key_id], 32);
	return 1;
}
