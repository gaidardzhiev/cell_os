/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gene/organelle.h"

typedef struct {
	uint32_t from_major, from_minor;
	uint32_t to_major,   to_minor;
} migrate_plan_t;

org_status_t migrate_handle(const migrate_plan_t* plan, const org_call_t* call, org_reply_t* reply);
