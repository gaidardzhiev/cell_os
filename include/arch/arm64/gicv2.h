/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdint.h>

void gicv2_init(void);
uint32_t gicv2_ack(void);
void gicv2_eoi(uint32_t intid);
void gicv2_enable(uint32_t intid);
