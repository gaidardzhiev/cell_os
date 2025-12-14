/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "core/parcel.h"
#include "core/obs.h"
#include "core/obs_trace.h"

static bool read_frame(FILE* f, uint8_t* b, size_t* L) {
	uint8_t le[4];
	if(fread(le,1,4,f)!=4)
		return false;
	size_t n=(size_t)le[0]|((size_t)le[1]<<8)|((size_t)le[2]<<16)|((size_t)le[3]<<24);
	if(n>*L)
		return false;
	if(fread(b,1,n,f)!=n)
		return false;
	*L=n;
	return true;
}

int main(void) {
	uint8_t buf[4096];
	obs_index_t idx; obs_index_init(&idx);
	while(1) {
		size_t L=sizeof buf; if(!read_frame(stdin, buf, &L)) break;
		parcel_hdr_t h; const uint8_t* p;
		if(!parcel_decode(buf, L, &h, &p)) { fprintf(stderr,"bad parcel\n"); return 2; }
		if(h.channel_id==0 && h.verb==2u) {
			if(h.len<20) { fprintf(stderr,"bad obs blob\n");
				return 2;
			} obs_index_feed(&idx, p, h.len);
			uint32_t kind = (uint32_t)p[8] | ((uint32_t)p[9]<<8) | ((uint32_t)p[10]<<16) | ((uint32_t)p[11]<<24);
			uint32_t plen = (uint32_t)p[16] | ((uint32_t)p[17]<<8) | ((uint32_t)p[18]<<16) | ((uint32_t)p[19]<<24);
			if(kind==OBS_REC_MARK && plen) {
				fwrite(p+20,1,plen,stdout);
			}
		}
	}
	fprintf(stderr,"#E8 OBS v2 replay=ok entries=%llu marks=%llu events=%llu sys=%llu traces=%llu\n",
			(unsigned long long)idx.entries,
			(unsigned long long)idx.marks,
			(unsigned long long)idx.events,
			(unsigned long long)idx.sys,
			(unsigned long long)idx.traces);
	fprintf(stderr,"#E8 OBS hash32=");
	for (int i=0;i<32;i++) fprintf(stderr,"%02x", idx.hash32[i]);
	fputc('\n', stderr);
	return 0;
}
