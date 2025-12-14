/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "core/parcel.h"
#include "core/obs.h"
#include "core/obs_trace.h"

static void rnd(uint8_t* b, size_t n) {
	for (size_t i=0;i<n;i++) b[i] = (uint8_t)rand();
}

int main(void) {
	srand(12345);
	for (int iter=0; iter<1000; iter++) {
		uint8_t buf[512]; rnd(buf, sizeof buf);
		parcel_hdr_t h; const uint8_t* p;
		(void)parcel_decode(buf, sizeof buf, &h, &p);
		uint8_t frame[512]; size_t L=0;
		(void)obs_mark_line(1, (uint32_t)iter, "#E9 fuzz\n", frame, sizeof frame, &L);
		if (L>=20){ frame[8] ^= 0xA5; frame[15] ^= 0x5A; }
		(void)parcel_decode(frame, L, &h, &p);
	}
	puts("#E9 fuzz ok");
	return 0;
}
