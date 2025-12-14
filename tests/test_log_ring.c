/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "core/log_ring.h"

static int sink(const uint8_t* buf, size_t len) {
	(void)buf;
	return (int)len;
}

int main(void) {
	uint8_t storage[65536];
	log_ring_t R;
	assert(log_ring_init(&R, storage, sizeof(storage)));
	uint8_t msg[16];
	for (size_t i = 0; i < sizeof(msg); ++i) msg[i] = (uint8_t)i;
	const uint32_t target = 1000000;
	for (uint32_t i = 0; i < target; ++i){
		assert(log_ring_enqueue(&R, msg, sizeof(msg)));
		if ((i % 256) == 0){
			(void)log_ring_drain(&R, sink);
		}
	}
	(void)log_ring_drain(&R, sink);
	assert(log_ring_drops(&R) == 0);
	puts("#C1 log ring test ok");
	return 0;
}
