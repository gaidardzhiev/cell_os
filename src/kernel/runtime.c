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
#include "core/ch0_log.h"

void* memset(void* dst, int c, size_t n) {
	uint8_t* d = (uint8_t*)dst;
	for (size_t i = 0; i < n; ++i) d[i] = (uint8_t)c;
	return dst;
}

void* memcpy(void* dst, const void* src, size_t n) {
	uint8_t* d = (uint8_t*)dst;
	const uint8_t* s = (const uint8_t*)src;
	for (size_t i = 0; i < n; ++i) d[i] = s[i];
	return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
	uint8_t* d = (uint8_t*)dst;
	const uint8_t* s = (const uint8_t*)src;
	if (d == s || n == 0)
		return dst;
	if (d < s) {
		for (size_t i = 0; i < n; ++i) d[i] = s[i];
	} else {
		for (size_t i = n; i-- > 0; ) d[i] = s[i];
	}
	return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
	const uint8_t* pa = (const uint8_t*)a;
	const uint8_t* pb = (const uint8_t*)b;
	for (size_t i = 0; i < n; ++i) {
		if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
	}
	return 0;
}

size_t strlen(const char* s) {
	size_t n = 0;
	while (s && s[n]) ++n;
	return n;
}

size_t strnlen(const char* s, size_t maxlen) {
	size_t n = 0;
	while (s && n < maxlen && s[n]) ++n;
	return n;
}

int strcmp(const char* a, const char* b){
	size_t i = 0;
	while (a[i] && b[i]){
		if (a[i] != b[i])
			return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
		++i;
	}
	return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
}

int strncmp(const char* a, const char* b, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		unsigned char ca = a[i], cb = b[i];
		if (ca != cb || ca == 0 || cb == 0)
			return (int)ca - (int)cb;
	}
	return 0;
}

char* getenv(const char* name) {
	(void)name;
	return NULL;
}

int ch0_write_parcel(const uint8_t* buf, size_t len) {
	(void)buf;
	return (int)len;
}

void ch0_log_line(const char* s) {
	(void)s;
}

void* __memcpy_chk(void* dst, const void* src, size_t len, size_t dstlen) {
	(void)dstlen;
	return memcpy(dst, src, len);
}

static unsigned short ctype_table[384];
static const unsigned short* ctype_ptr;

static void init_ctype_table(void) {
	if (ctype_ptr)
		return;
	for (int i = 0; i < 384; ++i) ctype_table[i] = 0;
	for (int ch = 0; ch < 256; ++ch) {
		unsigned short val = 0;
		if (ch >= 0x20 && ch <= 0x7E) val |= 0x40;
		if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f') val |= 0x20;
		ctype_table[ch + 128] = val;
	}
	ctype_ptr = ctype_table + 128;
}

const unsigned short** __ctype_b_loc(void) {
	init_ctype_table();
	return &ctype_ptr;
}
