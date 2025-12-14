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
#include "core/qos.h"
#include "core/channel.h"

typedef struct vm_handle vm_handle_t;

typedef struct {
	uint64_t cpu_budget_ns;
	uint8_t  priority;
	uint8_t  runnable;
} gene_budget_t;

typedef struct {
	uint32_t gene_id;
	vm_handle_t* vm;
	gene_budget_t budget;
} gene_task_t;

typedef struct {
	uint64_t quantum_ns;
	const channel_graph_t* cg;
} scheduler_t;

void sched_init(scheduler_t* s, const channel_graph_t* cg, uint64_t quantum_ns);
void sched_add(gene_task_t* t);
void sched_tick(scheduler_t* s);
qos_status_t sched_last_status(void);
