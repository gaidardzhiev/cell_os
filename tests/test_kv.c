/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/kv.h"

int main(void){
	assert(kv_init() == KV_OK);
	const char* ns = "mito";
	const char* key = "c0";
	uint8_t val[4] = {1,2,3,4};
	assert(kv_put(ns, key, val, sizeof(val)) == KV_OK);
	uint8_t out[8]; size_t len = sizeof(out);
	assert(kv_get(ns, key, out, &len) == KV_OK);
	assert(len == 4 && memcmp(out, val, 4) == 0);
	uint8_t val2[4] = {5,6,7,8};
	assert(kv_put(ns, key, val2, sizeof(val2)) == KV_OK);
	len = sizeof(out);
	assert(kv_get(ns, key, out, &len) == KV_OK);
	assert(len == 4 && memcmp(out, val2, 4) == 0);
	uint32_t crc = kv_crc32();
	assert(crc != 0);
	puts("#D2 kv sanity ok");
	return 0;
}
