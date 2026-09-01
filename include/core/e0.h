/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
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
