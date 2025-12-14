/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include "core/obs.h"

static void emit_len(FILE* f, unsigned n) {
		uint8_t le[4]={ n & 255, (n>>8)&255, (n>>16)&255, (n>>24)&255 };
		fwrite(le,1,4,f);
}

int main(void) {
		uint8_t frame[1024]; size_t fl=0;
		assert(obs_mark_line(1,1,"#E8 start\n", frame, sizeof frame, &fl));
		emit_len(stdout,(unsigned)fl); fwrite(frame,1,fl,stdout);
		assert(obs_mark_line(2,2,"#E8 middle\n", frame, sizeof frame, &fl));
		emit_len(stdout,(unsigned)fl); fwrite(frame,1,fl,stdout);
		return 0;
}
