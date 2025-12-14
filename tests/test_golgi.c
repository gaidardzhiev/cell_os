/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include "core/channel.h"
#include "gene/golgi_route.h"

extern void cg_build_full(channel_graph_t* g, channel_qos_t* qos_buf, uint8_t* adj_bs, uint32_t n, uint64_t refill, uint64_t bucket_max);

int main(void){
	channel_graph_t g; channel_qos_t qos[4]; uint8_t adj[32];
	cg_build_full(&g,qos,adj,4,1024,2048);
	golgi_ctx_t GG; golgi_init(&GG, &g);
	struct { uint32_t from,to; } req = { .from=0, .to=3 };
	org_call_t c = { .hdr = { .channel_id=0, .verb=1, .len=sizeof(req) },.payload = (const uint8_t*)&req };
	org_reply_t r;
	assert(golgi_handle(&GG,&c,&r)==ORG_OK);
	puts("#E5 golgi ok");
	return 0;
}
