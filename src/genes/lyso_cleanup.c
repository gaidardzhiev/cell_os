/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "gene/lyso_cleanup.h"

static inline void mark(lyso_ctx_t* l, uint32_t gid) {
	l->bits[(gid>>3)&255] |= (uint8_t)(1u<<(gid&7));
}

static inline int isdead(const lyso_ctx_t* l, uint32_t gid) {
	return (l->bits[(gid>>3)&255] >> (gid&7)) & 1u;
}

void lyso_init(lyso_ctx_t* l){
	memset(l,0,sizeof *l);
}

org_status_t lyso_handle(lyso_ctx_t* l, const org_call_t* c, org_reply_t* r) {
	if (!l || !c || c->hdr.len != 4) return ORG_ERR_FORMAT;
	uint32_t gid; memcpy(&gid, c->payload, 4);
	if (c->hdr.verb == 1u){
		mark(l,gid); uint8_t ok=1;
		return org_reply_pack(c->hdr.channel_id,1,&ok,1,r)?ORG_OK:ORG_ERR_INTERNAL;
	}
	if (c->hdr.verb == 2u){
		uint8_t v = (uint8_t)isdead(l,gid);
		return org_reply_pack(c->hdr.channel_id,2,&v,1,r)?ORG_OK:ORG_ERR_INTERNAL;
	}
	return ORG_ERR_DENY;
}
