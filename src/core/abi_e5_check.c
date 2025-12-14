/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "gene/organelle.h"
#include "gene/mito_energy.h"

_Static_assert(sizeof(mito_entry_t) == 32, "ABI drift: mito_entry_t");
_Static_assert(sizeof(org_reply_t) == 1032, "ABI drift: org_reply_t");
