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
#include <stdbool.h>
#include "core/crypto.h"

enum { MSG_FLAGS_NONE = 0u, CH_DIAG = 0u, VERB_LOG_LINE = 1u };

typedef struct {
	uint32_t channel_id;
	uint32_t verb;
	uint32_t flags;
	uint32_t len;
} parcel_hdr_t;

static inline void parcel_mac_stub(const uint8_t* buf, size_t len, uint8_t mac16[16]) {
	(void)buf; (void)len;
	for (int i = 0; i < 16; i++) mac16[i] = 0;
}

bool parcel_encode(uint8_t* out, size_t out_cap, uint32_t channel_id, uint32_t verb, uint32_t flags, const void* payload, uint32_t len, uint8_t mac16[16], size_t* out_len);

bool parcel_decode(const uint8_t* in, size_t in_len, parcel_hdr_t* hdr, const uint8_t** payload_out);

bool ch0_log_encode_line(const char* line, uint8_t* out, size_t cap, size_t* out_len);

static inline int parcel_sign(uint8_t* frame, size_t cap, size_t* inout_len, const mac_key_t* key) {
	if (!frame || !inout_len || !key)
		return 0;
	if (*inout_len + MAC16_LEN > cap)
		return 0;
	uint8_t tag[MAC16_LEN];
	mac16_compute(key, frame, *inout_len, tag);
	for (int i=0;i<MAC16_LEN;i++) frame[*inout_len + i] = tag[i];
	*inout_len += MAC16_LEN;
	return 1;
}

static inline int parcel_verify_and_strip(uint8_t* frame, size_t* inout_len, const mac_key_t* key) {
	if (!frame || !inout_len || !key)
		return 0;
	if (*inout_len < MAC16_LEN)
		return 0;
	size_t data_len = *inout_len - MAC16_LEN;
	uint8_t exp[MAC16_LEN];
	mac16_compute(key, frame, data_len, exp);
	if (!mac16_equal(exp, frame + data_len))
		return 0;
	*inout_len = data_len;
	return 1;
}
