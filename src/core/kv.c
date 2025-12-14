/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/kv.h"

#ifndef CFG_ENABLE_ORG_PKV
#define CFG_ENABLE_ORG_PKV 0
#endif

#define KV_MAX_ENTRIES 64
#define KV_MAX_KEY 32
#define KV_MAX_NS 16
#define KV_MAX_VAL 256

typedef struct {
	char ns[KV_MAX_NS];
	char key[KV_MAX_KEY];
	uint8_t val[KV_MAX_VAL];
	size_t len;
	bool used;
} kv_entry_t;

static kv_entry_t kv_store[KV_MAX_ENTRIES];
static bool kv_ready;

static size_t c_strlen(const char* s) {
	size_t n = 0;
	while (s && s[n]) ++n;
	return n;
}

static void copy_bytes(uint8_t* dst, const uint8_t* src, size_t n) {
	for (size_t i = 0; i < n; ++i) dst[i] = src[i];
}

static void copy_str(char* dst, size_t cap, const char* src) {
	size_t i = 0;
	while (i + 1 < cap && src && src[i] != '\0'){
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static bool str_eq_cap(const char* a, const char* b, size_t cap) {
	for (size_t i = 0; i < cap; ++i){
		char ca = a ? a[i] : 0;
		char cb = b ? b[i] : 0;
		if (ca != cb)
			return false;
		if (ca == '\0')
			return true;
	}
	return true;
}

static uint32_t crc32c(const uint8_t* p, size_t n) {
	uint32_t crc = 0xFFFFFFFFu;
	while (n--) {
		crc ^= *p++;
		for (int i = 0; i < 8; ++i) {
			uint32_t m = (uint32_t)-(int32_t)(crc & 1u);
			crc = (crc >> 1) ^ (0x82F63B78u & m);
		}
	}
	return crc ^ 0xFFFFFFFFu;
}

int kv_init(void) {
	for (size_t i = 0; i < KV_MAX_ENTRIES; ++i) kv_store[i].used = false;
	kv_ready = true;
	return KV_OK;
}

static kv_entry_t* find_slot(const char* ns, const char* key) {
	for (size_t i = 0; i < KV_MAX_ENTRIES; ++i) {
		kv_entry_t* e = &kv_store[i];
		if (e->used && str_eq_cap(e->ns, ns, KV_MAX_NS) && str_eq_cap(e->key, key, KV_MAX_KEY)){
			return e;
		}
	}
	for (size_t i = 0; i < KV_MAX_ENTRIES; ++i){
		kv_entry_t* e = &kv_store[i];
		if (!e->used)
			return e;
	}
	return NULL;
}

int kv_put(const char* ns, const char* key, const void* p, size_t n) {
	if (!kv_ready || !ns || !key || !p || n == 0 || n > KV_MAX_VAL) {
		return KV_ERR;
	}
	kv_entry_t* e = find_slot(ns, key);
	if (!e)
		return KV_ERR;
	copy_str(e->ns, KV_MAX_NS, ns);
	copy_str(e->key, KV_MAX_KEY, key);
	copy_bytes(e->val, (const uint8_t*)p, n);
	e->len = n;
	e->used = true;
	return KV_OK;
}

int kv_get(const char* ns, const char* key, void* p, size_t* n_inout) {
	if (!kv_ready || !ns || !key || !p || !n_inout)
		return KV_ERR;
	kv_entry_t* e = find_slot(ns, key);
	if (!e || !e->used)
		return KV_ERR;
	if (*n_inout < e->len)
		return KV_ERR;
	copy_bytes((uint8_t*)p, e->val, e->len);
	*n_inout = e->len;
	return KV_OK;
}

int kv_flush(void) {
	return kv_ready ? KV_OK : KV_ERR;
}

uint32_t kv_crc32(void) {
	if (!kv_ready)
		return 0;
	uint32_t crc = 0;
	for (size_t i = 0; i < KV_MAX_ENTRIES; ++i) {
		if (!kv_store[i].used)
			continue;
		crc ^= crc32c((const uint8_t*)kv_store[i].ns, c_strlen(kv_store[i].ns));
		crc ^= crc32c((const uint8_t*)kv_store[i].key, c_strlen(kv_store[i].key));
		crc ^= crc32c(kv_store[i].val, kv_store[i].len);
	}
	return crc;
}
