/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "gene/golgi_route.h"

typedef struct {
	uint32_t from, to;
} route_req_t;

void golgi_init(golgi_ctx_t* g, const channel_graph_t* cg) {
	g->cg = cg;
}

org_status_t golgi_handle(golgi_ctx_t* g, const org_call_t* c, org_reply_t* r) {
	if (!g || !c || c->hdr.len != sizeof(route_req_t))
		return ORG_ERR_FORMAT;
	route_req_t req; memcpy(&req, c->payload, sizeof(req));
	if (!cg_route_allowed(g->cg, req.from, req.to))
		return ORG_ERR_ROUTE;
	return org_reply_pack(c->hdr.channel_id, 1, &req, sizeof(req), r) ? ORG_OK : ORG_ERR_INTERNAL;
}
