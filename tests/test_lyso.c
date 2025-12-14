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
#include "gene/lyso_cleanup.h"

int main(void) {
	lyso_ctx_t L; lyso_init(&L);
	uint32_t gid=7;
	org_call_t c1={ .hdr={.channel_id=0,.verb=1,.len=4}, .payload=(const uint8_t*)&gid };
	org_call_t c2={ .hdr={.channel_id=0,.verb=2,.len=4}, .payload=(const uint8_t*)&gid };
	org_reply_t r;
	assert(lyso_handle(&L,&c1,&r)==ORG_OK);
	assert(lyso_handle(&L,&c2,&r)==ORG_OK);
	puts("#E5 lyso ok");
	return 0;
}
