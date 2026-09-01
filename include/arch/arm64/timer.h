/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool arm_timer_init_1ms(void);
void arm_timer_rearm(void);
void arm_timer_disable(void);
