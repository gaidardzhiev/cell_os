/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#ifndef CGF_E0_H
#define CGF_E0_H

#include "core/handoff.h"
#include "spec/cgf_substrate_db.h"



#ifdef __cplusplus
extern "C" {
#endif

void cgf_e0_probe_x86_64(handoff_t* h);
void cgf_e0_probe_arm64(handoff_t* h);

#ifdef __cplusplus
}
#endif

#endif 
