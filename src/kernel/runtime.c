/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include <stddef.h>

void *memset(void *dst, int c, size_t n) {
	unsigned char *p = (unsigned char *)dst;
	while (n--) *p++ = (unsigned char)c;
	return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	while (n--) *d++ = *s++;
	return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	if (d < s) while (n--) *d++ = *s++;
	else { d += n; s += n; while (n--) *--d = *--s; }
	return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
	const unsigned char *x = (const unsigned char *)a;
	const unsigned char *y = (const unsigned char *)b;
	while (n--) { if (*x != *y) return (int)*x - (int)*y; ++x; ++y; }
	return 0;
}
