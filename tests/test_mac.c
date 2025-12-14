/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include "core/crypto.h"

int main(void) {
	mac_key_t k; uint8_t raw[32]; for (int i=0;i<32;i++) raw[i]=(uint8_t)i;
	mac16_init(&k, 3, raw);
	const uint8_t msg[] = "parcel";
	uint8_t t1[16], t2[16];
	mac16_compute(&k, msg, sizeof msg, t1);
	mac16_compute(&k, msg, sizeof msg, t2);
	assert(mac16_equal(t1,t2));
	t2[0]^=1; assert(!mac16_equal(t1,t2));
	puts("#E9 mac ok");
	return 0;
}
