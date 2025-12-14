/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <ctype.h>
#include "gene/peroxi_sanitize.h"

org_status_t peroxi_handle(const org_call_t* c, org_reply_t* r){
	if (!c)
		return ORG_ERR_FORMAT;
	if (c->hdr.verb != 1u)
		return ORG_ERR_DENY;
	const uint8_t* p = c->payload;
	uint32_t n = c->hdr.len;
	uint8_t ok = 1;
	for (uint32_t i=0;i<n;i++){
		if (!(isprint(p[i]) || isspace(p[i]))){
			ok=0;
			break;
		}
	}
	return org_reply_pack(c->hdr.channel_id,1,&ok,1,r)?ORG_OK:ORG_ERR_INTERNAL;
}
