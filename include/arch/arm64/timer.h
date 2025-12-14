/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool arm_timer_init_1ms(void);
void arm_timer_rearm(void);
void arm_timer_disable(void);
