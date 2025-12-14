/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdio.h>
#include <stdint.h>
#include "core/obs_trace.h"

static int read_blob(FILE* f, uint8_t* b, size_t* L) {
	uint8_t le[4];
	if(fread(le,1,4,f)!=4)
		return 0;
	size_t n=(size_t)le[0]|((size_t)le[1]<<8)|((size_t)le[2]<<16)|((size_t)le[3]<<24);
	if(n>*L)
		return -1;
	if(fread(b,1,n,f)!=n)
		return -1;
	*L=n;
	return 1;
}

int main(void) {
	uint8_t buf[4096];
	obs_index_t idx; obs_index_init(&idx);
	while(1) {
		size_t L=sizeof buf;
		int r=read_blob(stdin, buf, &L);
		if(r<=0) break;
		obs_index_feed(&idx, buf, L);
	}
	printf("%llu %llu %llu %llu %llu\n",
			(unsigned long long)idx.entries,
			(unsigned long long)idx.marks,
			(unsigned long long)idx.events,
			(unsigned long long)idx.sys,
			(unsigned long long)idx.traces);
	for(int i=0;i<32;i++) printf("%02x", idx.hash32[i]);
	putchar('\n');
	return 0;
}
