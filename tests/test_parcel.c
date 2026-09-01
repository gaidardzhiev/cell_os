/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "core/parcel.h"

int main(void) {
	const char* line = "#E2 PARCEL v1\n";
	uint8_t buf[256]; size_t L=0;
	assert(ch0_log_encode_line(line, buf, sizeof(buf), &L));
	parcel_hdr_t h; const uint8_t* p = NULL;
	assert(parcel_decode(buf, L, &h, &p));
	assert(h.channel_id==CH_DIAG && h.verb==VERB_LOG_LINE);
	assert(h.len == strlen(line));
	assert(memcmp(p, line, h.len)==0);
	puts("#E2 test ok");
	return 0;
}
