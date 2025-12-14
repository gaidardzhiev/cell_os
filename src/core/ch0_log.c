/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdio.h>
#include <string.h>
#include "core/ch0_log.h"
#include "core/parcel.h"
#include "core/log_ring.h"

#ifndef CFG_ENABLE_NONBLOCK_LOG
#define CFG_ENABLE_NONBLOCK_LOG 0
#endif

#if CFG_ENABLE_NONBLOCK_LOG
static log_ring_t log_ring;
static uint8_t log_ring_buf[64 * 1024];
static int ch0_writer(const uint8_t* buf, size_t len) {
	return ch0_write_parcel(buf, len);
}

static void ensure_ring(void) {
	static int inited;
	if (!inited){
		log_ring_init(&log_ring, log_ring_buf, sizeof(log_ring_buf));
		inited = 1;
	}
}

static void emit_drops_if_any(void) {
	const uint32_t drops = log_ring_drops(&log_ring);
	if (drops == 0)
		return;
	char drop_line[64];
	int n = snprintf(drop_line, sizeof(drop_line), "#C2 log ring drops=%u\n", drops);
	if (n <= 0)
		return;
	size_t dn = (size_t)n;
	if (dn > sizeof(drop_line)) dn = sizeof(drop_line);
	log_ring_enqueue(&log_ring, (const uint8_t*)drop_line, dn);
}
#endif

int ch0_write_parcel(const uint8_t* buf, size_t len) {
	return (int)fwrite(buf, 1, len, stdout);
}

void ch0_log_line(const char* s) {
	uint8_t frame[1024];
	size_t flen = 0;
	if (ch0_log_encode_line(s, frame, sizeof(frame), &flen)) {
#if CFG_ENABLE_NONBLOCK_LOG
		ensure_ring();
		(void)log_ring_enqueue(&log_ring, frame, flen);
		emit_drops_if_any();
		(void)log_ring_drain(&log_ring, ch0_writer);
#else
		ch0_write_parcel(frame, flen);
#endif
	}
}
