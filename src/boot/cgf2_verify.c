/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stddef.h>
#include <stdint.h>
#include "boot/cgf2.h"
#include "core/hash.h"

static inline void out8(uint16_t port, uint8_t v) {
	__asm__ volatile("outb %0, %1" :: "a"(v), "Nd"(port));
}

static inline void dbg_putc(char c) {
	out8(0xE9, (uint8_t)c);
	out8(0x3F8, (uint8_t)c);
}

static void dbg_puts(const char* s) {
	while (*s) dbg_putc(*s++);
}

static void dbg_hex32(uint32_t v) {
	for (int i = 7; i >= 0; --i) {
		uint8_t nyb = (uint8_t)((v >> (i * 4)) & 0xF);
		dbg_putc((char)(nyb < 10 ? '0' + nyb : 'A' + (nyb - 10)));
	}
}

static void dbg_hex_sha(const uint8_t sha[32]) {
	for (int i = 0; i < 32; ++i) {
		uint8_t hi = (uint8_t)((sha[i] >> 4) & 0xF);
		uint8_t lo = (uint8_t)(sha[i] & 0xF);
		dbg_putc((char)(hi < 10 ? '0' + hi : 'A' + (hi - 10)));
		dbg_putc((char)(lo < 10 ? '0' + lo : 'A' + (lo - 10)));
	}
}

static uint32_t crc32c_calc(const uint8_t* p, size_t n) {
	uint32_t crc = 0xFFFFFFFFu;
	while (n--) {
		crc ^= *p++;
		for (int i = 0; i < 8; ++i) {
			uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
			crc = (crc >> 1) ^ (0x82F63B78u & mask);
		}
	}
	return crc ^ 0xFFFFFFFFu;
}

static void log_header_crc(uint32_t crc) {
	dbg_puts("#A1 cgf hdr crc32c=");
	dbg_hex32(crc);
	dbg_putc('\n');
}

static void log_sha(const uint8_t sha[32]) {
	dbg_puts("#A2 genedir sha256=");
	dbg_hex_sha(sha);
	dbg_putc('\n');
}

static void log_ok(void) {
	dbg_puts("#A3 cgf verify ok\n");
}

static void log_fail(void) {
	dbg_puts("#A! cgf verify fail\n");
}

int cgf2_verify_and_log(const uint8_t* base, size_t len) {
	if (!base || len < sizeof(cgf2_header_t)) {
		log_fail();
		return 1;
	}
	const cgf2_header_t* hdr = (const cgf2_header_t*)base;
	uint8_t header_buf[sizeof(cgf2_header_t)];
	for (size_t i = 0; i < sizeof(cgf2_header_t); ++i) header_buf[i] = base[i];
	header_buf[0] = header_buf[1] = header_buf[2] = header_buf[3] = 0;
	uint32_t crc = crc32c_calc(header_buf, sizeof(cgf2_header_t));
	log_header_crc(crc);
	if (crc != hdr->crc32c) {
		log_fail();
		return 1;
	}
	uint32_t off = hdr->genedir_off;
	uint32_t glen = hdr->genedir_len;
	if ((size_t)off + (size_t)glen > len || glen == 0) {
		log_fail();
		return 1;
	}
	uint8_t sha[32];
	cell_sha256(base + off, glen, sha);
	log_sha(sha);
	log_ok();
	return 0;
}
