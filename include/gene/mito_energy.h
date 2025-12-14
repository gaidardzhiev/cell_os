/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gene/organelle.h"

typedef struct {
	uint32_t gene_id;
	uint64_t cpu_used_ns;
	uint64_t io_used_bytes;
	uint64_t mem_used_bytes;
} mito_entry_t;

typedef struct {
	mito_entry_t* entries;
	uint32_t cap;
	uint32_t len;
} mito_table_t;

void mito_init(mito_table_t* t, mito_entry_t* store, uint32_t cap);
void mito_charge_cpu(mito_table_t* t, uint32_t gene_id, uint64_t ns);
void mito_charge_io (mito_table_t* t, uint32_t gene_id, uint64_t bytes);
void mito_charge_mem(mito_table_t* t, uint32_t gene_id, uint64_t bytes);
bool mito_get(const mito_table_t* t, uint32_t gene_id, mito_entry_t* out);
org_status_t mito_handle(mito_table_t* t, uint32_t gene_id, const org_call_t* call, org_reply_t* reply);
