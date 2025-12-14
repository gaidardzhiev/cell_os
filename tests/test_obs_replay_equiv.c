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
#include "core/obs.h"
#include "core/parcel.h"

int main(void) {
	uint8_t f1[1024], f2[1024]; size_t l1=0,l2=0;
	obs_hdr_t h = {2,0,1,10,42,3};
	assert(obs_pack_to_ch0(&h, "E8\n", f1, sizeof f1, &l1));
	assert(obs_pack_to_ch0(&h, "E8\n", f2, sizeof f2, &l2));
	assert(l1==l2 && memcmp(f1,f2,l1)==0);
	puts("#E8 replay eq ok");
	return 0;
}
