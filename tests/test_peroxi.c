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
#include "gene/peroxi_sanitize.h"

int main(void){
	const char* good="Hello, Cell OS!\n";
	const uint8_t bad[] = { 'O','K','\n', 0x01, 0xFF };
	org_reply_t r; org_call_t c;
	c.hdr=(msg_t){.channel_id=0,.verb=1,.len=(uint32_t)strlen(good)};
	c.payload=(const uint8_t*)good;
	assert(peroxi_handle(&c,&r)==ORG_OK);
	c.hdr=(msg_t){.channel_id=0,.verb=1,.len=(uint32_t)sizeof(bad)};
	c.payload=bad;
	assert(peroxi_handle(&c,&r)==ORG_OK);
	puts("#E5 peroxi ok");
	return 0;
}
