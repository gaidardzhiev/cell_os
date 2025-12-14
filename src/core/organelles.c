/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include <stdlib.h>
#include "core/organelles.h"
#include "core/ch0_log.h"
#include "core/parcel.h"

#define MITO_CAPACITY 64u

static mito_entry_t g_mito_store[MITO_CAPACITY];
static mito_table_t g_mito;
static golgi_ctx_t g_golgi;
static lyso_ctx_t g_lyso;
static bool g_init;

static bool lyso_dead(const lyso_ctx_t* l, uint32_t gid) {
	return (l->bits[(gid >> 3) & 255u] >> (gid & 7u)) & 1u;
}

static void lyso_mark(lyso_ctx_t* l, uint32_t gid) {
	l->bits[(gid >> 3) & 255u] |= (uint8_t)(1u << (gid & 7u));
}

static int org_verbose(void) {
	const char* v = getenv("CELL_VERBOSE");
	if (!v || !*v) {
		return 0;
	}
	return !(v[0] == '0' && v[1] == '\0');
}

static bool peroxi_accepts(const char* line) {
	if (!line) {
		return false;
	}
	const size_t len = strlen(line);
	org_call_t call = {
		.hdr = {
			.channel_id = 0u,
			.verb = 1u,
			.len = (uint32_t)len,
		},
		.payload = (const uint8_t*)line,
	};
	org_reply_t reply = {0};
	if (peroxi_handle(&call, &reply) != ORG_OK) {
		return false;
	}
	parcel_hdr_t hdr;
	const uint8_t* payload = NULL;
	if (!parcel_decode(reply.buf, reply.len, &hdr, &payload) || !payload || hdr.len == 0) {
		return false;
	}
	return payload[0] != 0;
}

bool organelles_sanitize_and_log(const char* line) {
	if (!line) {
		return false;
	}
	if (!peroxi_accepts(line)) {
		return false;
	}
	ch0_log_line(line);
	return true;
}

void organelles_init(const channel_graph_t* cg) {
	mito_init(&g_mito, g_mito_store, MITO_CAPACITY);
	golgi_init(&g_golgi, cg);
	lyso_init(&g_lyso);
	g_init = true;
	if (org_verbose()) {
		(void)organelles_sanitize_and_log("#E5 ORG ok\n");
	}
}

void organelles_charge_cpu(uint32_t gene_id, uint64_t ns) {
	if (!g_init) return;
	mito_charge_cpu(&g_mito, gene_id, ns);
}

void organelles_charge_io(uint32_t gene_id, uint64_t bytes) {
	if (!g_init) return;
	mito_charge_io(&g_mito, gene_id, bytes);
}

void organelles_charge_mem(uint32_t gene_id, uint64_t bytes) {
	if (!g_init) return;
	mito_charge_mem(&g_mito, gene_id, bytes);
}

bool organelles_route_allowed(uint32_t from, uint32_t to) {
	if (!g_init || !g_golgi.cg) {
		return false;
	}
	if (from >= g_golgi.cg->num_channels || to >= g_golgi.cg->num_channels) {
		return false;
	}
	if (cg_route_allowed(g_golgi.cg, from, to)) {
		return true;
	}
	return from == to;
}

void organelles_mark_dead(uint32_t gene_id) {
	if (!g_init) return;
	lyso_mark(&g_lyso, gene_id);
}

bool organelles_is_dead(uint32_t gene_id) {
	if (!g_init) return false;
	return lyso_dead(&g_lyso, gene_id);
}

const mito_table_t* organelles_mito_table(void) {
	return g_init ? &g_mito : NULL;
}
