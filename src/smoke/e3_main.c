/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "core/log_marker.h"
#include "core/parcel.h"
#include "core/cg.h"
#include "core/qos.h"

#if defined(__x86_64__)
static inline void outb(uint16_t port, uint8_t val) {
		__asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
		uint8_t ret;
		__asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
		return ret;
}

static void com1_write(const char* s, size_t len) {
		for (size_t i = 0; i < len; ++i) {
	uint8_t ch = (uint8_t)s[i];
	while ((inb(0x3F8 + 5) & 0x20u) == 0u) {
	}
	outb(0x3F8, ch);
		}
}
#endif

static void output_line(const char* line) {
		uint8_t frame[256];
		size_t frame_len = 0;
		bool encoded = ch0_log_encode_line(line, frame, sizeof(frame), &frame_len);

		char buf[256];
		size_t copy = 0;

		if (encoded) {
	parcel_hdr_t hdr;
	const uint8_t* payload = NULL;
	if (parcel_decode(frame, frame_len, &hdr, &payload) && payload && hdr.len > 0) {
			copy = (hdr.len < sizeof(buf) - 1) ? hdr.len : sizeof(buf) - 1;
			for (size_t i = 0; i < copy; ++i) {
		buf[i] = (char)payload[i];
			}
	}
		}

		if (copy == 0) {
	const char* src = line ? line : "";
	while (src[copy] != '\0' && copy < sizeof(buf) - 2) {
			buf[copy] = src[copy];
			++copy;
	}
	if (copy == 0 || buf[copy - 1] != '\n') {
			buf[copy++] = '\n';
	}
		}
		buf[copy] = '\0';

#if defined(__x86_64__)
		com1_write(buf, copy);
		log_marker_port_e9(buf);
#elif defined(__aarch64__)
		log_marker_pl011(buf);
#endif
}

static void emit_status(qos_status_t st) {
		switch (st) {
	case QOS_OK:   output_line("#E3 CG ok\n");   break;
	case QOS_BUSY: output_line("#E3 CG busy\n"); break;
	case QOS_EGAS: output_line("#E3 CG egas\n"); break;
	default:       break;
		}
}

int e3_main(void) {
	static const cg_entry_t entries[] = {
		{ .chan = 0, .sink_id = CG_SINK_DIAG, .max_depth = 1, .rsv = 0 },
	};
	const cg_table_t table = { entries, 1 };
	cg_entry_t route = entries[0];
	if (cg_init(&table)) {
		(void)cg_route(0, &route);
	}
	qos_init(1);
	qos_cfg_channel(route.chan, QOS_BE, route.max_depth, 1024, 512);
	qos_set_gas(1024);
	emit_status(qos_try_enqueue(route.chan, 128));
	emit_status(qos_try_enqueue(route.chan, 128));
	qos_set_gas(0);
	emit_status(qos_try_enqueue(route.chan, 64));
	for (;;) {
#if defined(__x86_64__)
		__asm__ volatile ("hlt");
#elif defined(__aarch64__)
		__asm__ volatile ("wfi");
#else
		break;
#endif
	}
	return 0;
}
