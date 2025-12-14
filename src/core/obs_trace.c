/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/obs_trace.h"
#include "core/obs.h"

static void fake_hash32(const uint8_t* buf, size_t len, uint8_t out[32]) {
	uint32_t s = 0x6F627332u;
	for(size_t i=0;i<len;i++) s = (s<<5) ^ (s>>2) ^ buf[i];
	for(int i=0;i<32;i++) out[i] ^= (uint8_t)((s >> ((i&3)*8)) & 0xFF);
}

void obs_index_init(obs_index_t* i) {
	memset(i, 0, sizeof *i);
}

void obs_index_feed(obs_index_t* i, const uint8_t* blob, size_t len) {
	if (!i || !blob || len < 20)
		return;
	uint32_t kind = (uint32_t)blob[8]  | ((uint32_t)blob[9]<<8) | ((uint32_t)blob[10]<<16) | ((uint32_t)blob[11]<<24);
	uint32_t plen = (uint32_t)blob[16] | ((uint32_t)blob[17]<<8) | ((uint32_t)blob[18]<<16) | ((uint32_t)blob[19]<<24);
	if (20u + plen != len)
		return;
	i->entries++;
	if(kind == OBS_REC_MARK) i->marks++;
	else if(kind == OBS_REC_EVENT) i->events++;
	else if(kind == OBS_REC_SYS) i->sys++;
	else if(kind == OBS_REC_TRACE) i->traces++;
	fake_hash32(blob, len, i->hash32);
}
