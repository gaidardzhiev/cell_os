/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "gene/mito_energy.h"

static mito_entry_t* find_or_add(mito_table_t* t, uint32_t gid) {
	for (uint32_t i=0;i<t->len;i++) if (t->entries[i].gene_id==gid)
		return &t->entries[i];
	if (t->len < t->cap){
		mito_entry_t* e = &t->entries[t->len++];
		memset(e,0,sizeof *e);
		e->gene_id = gid;
		return e;
	}
	return NULL;
}

void mito_init(mito_table_t* t, mito_entry_t* store, uint32_t cap) {
	t->entries = store; t->cap = cap; t->len = 0;
}
void mito_charge_cpu(mito_table_t* t, uint32_t gid, uint64_t ns) {
	mito_entry_t* e = find_or_add(t,gid);
	if (e) e->cpu_used_ns += ns;
}
void mito_charge_io (mito_table_t* t, uint32_t gid, uint64_t b) {
	mito_entry_t* e = find_or_add(t,gid);
	if (e) e->io_used_bytes += b;
}
void mito_charge_mem(mito_table_t* t, uint32_t gid, uint64_t b) {
	mito_entry_t* e = find_or_add(t,gid);
	if (e) e->mem_used_bytes += b;
}
bool mito_get(const mito_table_t* t, uint32_t gid, mito_entry_t* out) {
	for (uint32_t i=0;i<t->len;i++)
		if (t->entries[i].gene_id==gid){
			if (out) *out = t->entries[i];
			return true;
		}
	return false;
}

org_status_t mito_handle(mito_table_t* t, uint32_t gid, const org_call_t* c, org_reply_t* r) {
	(void)c;
	mito_entry_t e;
	if (!mito_get(t,gid,&e)) {
		memset(&e,0,sizeof e); e.gene_id=gid;
	}
	return org_reply_pack(0, 1, &e, (uint32_t) sizeof(e), r) ? ORG_OK : ORG_ERR_INTERNAL;
}
