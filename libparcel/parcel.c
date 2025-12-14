/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/parcel.h"

static inline void le32_store(uint8_t* p, uint32_t v){
	p[0]=(uint8_t)(v); p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

static inline void byte_copy(uint8_t* dst, const uint8_t* src, size_t len) {
	for (size_t i = 0; i < len; ++i) {
		dst[i] = src[i];
	}
}

bool parcel_encode(uint8_t* out, size_t out_cap,uint32_t channel_id, uint32_t verb, uint32_t flags, const void* payload, uint32_t len, uint8_t mac16[16], size_t* out_len) {
	if (!out || !out_len)
		return false;
	const size_t HDR = 16u;
	if (HDR + (size_t)len > out_cap)
		return false;
	le32_store(out+0,  channel_id);
	le32_store(out+4,  verb);
	le32_store(out+8,  flags);
	le32_store(out+12, len);
	if (len && payload) byte_copy(out+HDR, (const uint8_t*)payload, len);
	if (mac16) parcel_mac_stub(out, HDR + (size_t)len, mac16);
	*out_len = HDR + (size_t)len;
	return true;
}

bool parcel_decode(const uint8_t* in, size_t in_len, parcel_hdr_t* hdr, const uint8_t** payload_out) {
	if (!in || !hdr || in_len < 16u)
		return false;
	const uint8_t* p = in;
	uint32_t ch = (uint32_t)p[0]  | ((uint32_t)p[1]<<8)  | ((uint32_t)p[2]<<16)  | ((uint32_t)p[3]<<24);
	uint32_t vb = (uint32_t)p[4]  | ((uint32_t)p[5]<<8)  | ((uint32_t)p[6]<<16)  | ((uint32_t)p[7]<<24);
	uint32_t fl = (uint32_t)p[8]  | ((uint32_t)p[9]<<8)  | ((uint32_t)p[10]<<16) | ((uint32_t)p[11]<<24);
	uint32_t ln = (uint32_t)p[12] | ((uint32_t)p[13]<<8) | ((uint32_t)p[14]<<16) | ((uint32_t)p[15]<<24);
	if (16u + (size_t)ln != in_len)
		return false;
	hdr->channel_id = ch; hdr->verb = vb; hdr->flags = fl; hdr->len = ln;
	if (payload_out) *payload_out = (ln ? in + 16 : NULL);
	return true;
}

bool ch0_log_encode_line(const char* line, uint8_t* out, size_t cap, size_t* out_len){
	if (!line) line = "";
	size_t n = 0;
	while (line[n] != '\0') ++n;
	const bool add_nl = (n == 0 || line[n-1] != '\n');
	uint8_t tmp[1024];
	if (n + (add_nl?1u:0u) > sizeof(tmp))
		return false;
	for (size_t i = 0; i < n; ++i) {
		tmp[i] = (uint8_t)line[i];
	}
	if (add_nl) tmp[n++] = (uint8_t)'\n';
	uint8_t mac[16];
	return parcel_encode(out, cap, CH_DIAG, VERB_LOG_LINE, MSG_FLAGS_NONE, tmp, (uint32_t)n, mac, out_len);
}
