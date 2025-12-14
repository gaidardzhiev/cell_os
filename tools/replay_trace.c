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

static bool read_frame(FILE* f, uint8_t* buf, size_t* len) {
	uint8_t le[4];
	if (fread(le,1,4,f) != 4)
		return false;
	size_t L = (size_t)le[0] | ((size_t)le[1]<<8) | ((size_t)le[2]<<16) | ((size_t)le[3]<<24);
	if (L > *len)
		return false;
	if (fread(buf,1,L,f) != L)
		return false;
	*len = L;
	return true;
}

int main(void) {
	uint8_t buf[4096];
	while (1){
		size_t len = sizeof(buf);
		if (!read_frame(stdin, buf, &len)) break;
		parcel_hdr_t h; const uint8_t* p = NULL;
		if (!parcel_decode(buf, len, &h, &p)) {
			fprintf(stderr,"bad frame\n");
			return 2;
		}
		if (h.channel_id==CH_DIAG && h.verb==VERB_LOG_LINE && p) fwrite(p,1,h.len,stdout);
	}
	return 0;
}
