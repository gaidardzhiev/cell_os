/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>

typedef enum {
	E_OK = 0,
	E_HDR = 1,
	E_IDX = 2,
	E_P_SIG = 3,
	E_NO_PHENOTYPE = 4,
	E_NO_CAP = 5,
	E_BUDGET = 6,
	E_DENY = 7,
	E_VERIFY = 8,
	E_TRAP = 9,
} error_code_t;
