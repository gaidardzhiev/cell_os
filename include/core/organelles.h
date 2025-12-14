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
#include "core/channel.h"
#include "gene/organelle.h"
#include "gene/mito_energy.h"
#include "gene/golgi_route.h"
#include "gene/lyso_cleanup.h"
#include "gene/peroxi_sanitize.h"

void organelles_init(const channel_graph_t* cg);
void organelles_charge_cpu(uint32_t gene_id, uint64_t ns);
void organelles_charge_io(uint32_t gene_id, uint64_t bytes);
void organelles_charge_mem(uint32_t gene_id, uint64_t bytes);
bool organelles_route_allowed(uint32_t from, uint32_t to);
void organelles_mark_dead(uint32_t gene_id);
bool organelles_is_dead(uint32_t gene_id);
bool organelles_sanitize_and_log(const char* line);
const mito_table_t* organelles_mito_table(void);
