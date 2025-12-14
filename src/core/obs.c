/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/obs.h"

static inline void le32(uint8_t* p, uint32_t v) {
	p[0]=(uint8_t)v;
	p[1]=(uint8_t)(v>>8);
	p[2]=(uint8_t)(v>>16);
	p[3]=(uint8_t)(v>>24);
}

bool obs_pack_to_ch0(const obs_hdr_t* h, const void* payload, uint8_t* out, size_t cap, size_t* out_len) {
	if(!h||!out||!out_len) return false;
	uint32_t n = 20u + h->payload_len;
	if(n > 1800u) return false;
	uint8_t blob[2048];
	le32(blob+0,  h->ver_major);
	le32(blob+4,  h->ver_minor);
	le32(blob+8,  h->rec_kind);
	le32(blob+12, h->tick);
	le32(blob+16, h->seq);
	if(h->payload_len && payload) memcpy(blob+20, payload, h->payload_len);
	return parcel_encode(out, cap, 0u, 2u, 0u, blob, n, NULL, out_len);
}

bool obs_mark_line(uint32_t tick, uint32_t seq, const char* line, uint8_t* out, size_t cap, size_t* out_len) {
	char tmp[512];
	const char* s = line ? line : "";
	size_t L = 0;
	while(s[L] && L < sizeof(tmp) - 2){ tmp[L] = s[L]; L++; }
	if(L == 0 || tmp[L-1] != '\n'){ tmp[L++] = '\n'; }
	tmp[L] = 0;
	obs_hdr_t h = { OBS_VER_MAJOR, OBS_VER_MINOR, OBS_REC_MARK, tick, seq, (uint32_t)L };
	return obs_pack_to_ch0(&h, tmp, out, cap, out_len);
}
