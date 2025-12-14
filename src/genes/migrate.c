/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "gene/migrate.h"

typedef struct { uint8_t ok; } migrate_resp_t;

org_status_t migrate_handle(const migrate_plan_t* plan, const org_call_t* call, org_reply_t* reply) {
	if (!plan || !call)
		return ORG_ERR_FORMAT;
	if (call->hdr.verb!=1u && call->hdr.verb!=2u)
		return ORG_ERR_DENY;
	if (call->hdr.verb==2u){
		if (call->hdr.len != sizeof(migrate_plan_t))
			return ORG_ERR_FORMAT;
		migrate_plan_t in;
		memcpy(&in, call->payload, sizeof(in));
		if (memcmp(&in, plan, sizeof(in))!=0)
			return ORG_ERR_DENY;
	}
	migrate_resp_t r = { .ok = 1 };
	return org_reply_pack(call->hdr.channel_id, call->hdr.verb, &r, (uint32_t)sizeof r, reply) ? ORG_OK : ORG_ERR_INTERNAL;
}
