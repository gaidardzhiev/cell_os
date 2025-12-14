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
#include "gene/migrate.h"

int main(void) {
	migrate_plan_t plan = { .from_major=1,.from_minor=0, .to_major=1,.to_minor=1 };
	org_reply_t r; org_call_t c;
	c.hdr=(msg_t){.channel_id=0,.verb=1,.len=0};
	c.payload=NULL;
	assert(migrate_handle(&plan,&c,&r)==ORG_OK);
	c.hdr=(msg_t){.channel_id=0,.verb=2,.len=(uint32_t)sizeof(plan)};
	c.payload=(const uint8_t*)&plan;
	assert(migrate_handle(&plan,&c,&r)==ORG_OK);
	migrate_plan_t wrong = plan; wrong.to_minor=2;
	c.hdr.len=(uint32_t)sizeof(wrong);
	c.payload=(const uint8_t*)&wrong;
	assert(migrate_handle(&plan,&c,&r)!=ORG_OK);
	puts("#E7 migrate ok");
	return 0;
}
