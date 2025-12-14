/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "core/parcel.h"

int main(void) {
	const char* line = "#E2 PARCEL v1\n";
	uint8_t buf[1024];
	size_t len = 0;
	if (!ch0_log_encode_line(line, buf, sizeof(buf), &len)){
		fprintf(stderr, "encode failed\n");
		return 1;
	}
	uint32_t le_len = (uint32_t)len;
	uint8_t hdr[4] = {
		(uint8_t)(le_len & 0xFFu),
		(uint8_t)((le_len >> 8) & 0xFFu),
		(uint8_t)((le_len >> 16) & 0xFFu),
		(uint8_t)((le_len >> 24) & 0xFFu)
	};
	if (fwrite(hdr, 1, sizeof(hdr), stdout) != sizeof(hdr))
		return 2;
	if (fwrite(buf, 1, len, stdout) != len)
		return 3;
	return 0;
}
