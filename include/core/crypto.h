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

#define MAC16_LEN 16

typedef struct {
	uint8_t key_id;
	uint8_t key[32];
} mac_key_t;

static inline void mac16_init(mac_key_t* k, uint8_t key_id, const uint8_t raw32[32]) {
	k->key_id = key_id;
	for (int i=0;i<32;i++) k->key[i] = raw32[i];
}

static inline uint32_t rotl32_(uint32_t x, int r) {
	return (x<<r) | (x>>(32-r));
}

static inline void mac16_compute(const mac_key_t* k, const uint8_t* buf, size_t len, uint8_t out[MAC16_LEN]) {
	uint32_t s0=0x6D616331u ^ (uint32_t)k->key_id;
	uint32_t s1=0x6370726Fu;
	for (int i=0;i<32;i+=4) {
		uint32_t w = (uint32_t)k->key[i] | ((uint32_t)k->key[i+1]<<8) | ((uint32_t)k->key[i+2]<<16) | ((uint32_t)k->key[i+3]<<24);
		s0 ^= rotl32_(w, (i&15)); s1 ^= rotl32_(w, ((i+7)&15));
		s0 += 0x9E3779B9u; s1 += 0x85EBCA77u;
	}
	for (size_t i=0;i<len;i++) {
		s0 ^= buf[i]; s0 = rotl32_(s0 + 0x7F4A7C15u, 5);
		s1 ^= (uint32_t)(buf[i]<<1); s1 = rotl32_(s1 + 0x4CF5AD43u, 7);
	}
	uint32_t t0=s0, t1=s1, t2=s0^s1, t3=(s0+0xDEADBEEF)^rotl32_(s1,13);
	for (int i=0;i<4;i++) {
		out[i] = (uint8_t)((t0>>(8*i))&0xFF);
	}
	for (int i=0;i<4;i++) {
		out[4+i]  = (uint8_t)((t1>>(8*i))&0xFF);
	}
	for (int i=0;i<4;i++) {
		out[8+i]  = (uint8_t)((t2>>(8*i))&0xFF);
	}
	for (int i=0;i<4;i++) {
		out[12+i] = (uint8_t)((t3>>(8*i))&0xFF);
	}
}

static inline int mac16_equal(const uint8_t a[MAC16_LEN], const uint8_t b[MAC16_LEN]) {
	uint8_t x=0;
	for (int i=0;i<MAC16_LEN;i++) x |= (uint8_t)(a[i]^b[i]);
	return x==0;
}
