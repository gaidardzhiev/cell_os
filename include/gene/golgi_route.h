/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>
#include <string.h>
#include "gene/organelle.h"

typedef struct {
	const channel_graph_t* cg;
} golgi_ctx_t;

void golgi_init(golgi_ctx_t* g, const channel_graph_t* cg);

org_status_t golgi_handle(golgi_ctx_t* g, const org_call_t* call, org_reply_t* reply);
