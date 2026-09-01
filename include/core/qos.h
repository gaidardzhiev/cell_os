/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum { QOS_BE = 0, QOS_RT = 1 } qos_class_t;
typedef enum { QOS_OK = 0, QOS_BUSY = 1, QOS_DROP = 2, QOS_EGAS = 3 } qos_status_t;

void qos_init(uint32_t num_channels);
void qos_cfg_channel(uint16_t chan, qos_class_t cls, uint32_t max_depth, uint64_t bucket_max, uint64_t refill_per_tick);
void qos_set_gas(uint32_t gas);
uint32_t qos_get_gas(void);

qos_status_t qos_try_enqueue(uint16_t chan, uint32_t bytes_cost);
void qos_tick(void);
