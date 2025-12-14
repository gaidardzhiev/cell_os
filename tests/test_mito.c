/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include "gene/mito_energy.h"

int main(void) {
	mito_entry_t store[8]; mito_table_t T; mito_init(&T, store, 8);
	mito_charge_cpu(&T, 42, 1000);
	mito_charge_io (&T, 42, 4096);
	mito_charge_mem(&T, 42, 512);
	org_call_t c = { .hdr = { .channel_id=0, .verb=1, .len=0 }, .payload=NULL };
	org_reply_t r;
	assert(mito_handle(&T,42,&c,&r)==ORG_OK);
	printf("#E5 mito ok len=%zu\n", r.len);
	return 0;
}
