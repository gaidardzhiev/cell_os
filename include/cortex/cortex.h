/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "cortex/cwm.h"

typedef struct {
	cwm_model_t model;
	uint32_t pos;
	float *x;
	float *q;
	float *tmp;
	float *ff;
	float *logits;
	float *scores;
	float *k_cache;
	float *v_cache;
} cortex_t;

size_t cortex_workspace_bytes(const cwm_model_t *m);
int cortex_init(cortex_t *c, const cwm_model_t *m, void *workspace, size_t workspace_bytes);
void cortex_reset(cortex_t *c);
int cortex_feed(cortex_t *c, uint8_t token);
uint8_t cortex_next(cortex_t *c);
