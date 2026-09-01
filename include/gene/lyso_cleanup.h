/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stdint.h>
#include "gene/organelle.h"

typedef struct {
	uint8_t bits[256];
} lyso_ctx_t;

void lyso_init(lyso_ctx_t* l);

org_status_t lyso_handle(lyso_ctx_t* l, const org_call_t* call, org_reply_t* reply);
