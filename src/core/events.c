/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/events.h"

static inline void le32(uint8_t* p, uint32_t v){
	p[0]=(uint8_t)(v); p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

bool event_pack(event_frame_t* out, uint32_t seq, uint32_t src, event_kind_t kind, const void* payload, uint32_t len) {
	if (!out)
		return false;
	if (sizeof(event_hdr_t) + (size_t)len > sizeof(out->buf))
		return false;
	uint8_t* p = out->buf;
	le32(p+0,  seq);
	le32(p+4,  src);
	le32(p+8,  (uint32_t)kind);
	le32(p+12, len);
	if (len && payload) memcpy(p+16, payload, len);
	out->len = 16u + (size_t)len;
	return true;
}
