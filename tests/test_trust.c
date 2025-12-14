/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include "core/trust.h"

int main(void) {
	uint8_t ks[2][32]={{0}};
	for (int i=0;i<32;i++) {
		ks[0][i]=i; ks[1][i]=31-i;
	}
	trust_set_t T; trust_init(&T, ks, 2, 1, 7);
	uint8_t id, raw[32]; assert(trust_active_mac(&T, &id, raw)==1 && id==1);
	trust_rotate(&T, 0);
	assert(T.active_key_id==0 && T.version==8);
	puts("#E9 trust ok");
	return 0;
}
