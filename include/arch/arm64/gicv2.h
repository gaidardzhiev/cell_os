/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once

#include <stdint.h>

void gicv2_init(void);
uint32_t gicv2_ack(void);
void gicv2_eoi(uint32_t intid);
void gicv2_enable(uint32_t intid);
