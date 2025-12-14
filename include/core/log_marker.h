/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>

#ifdef __x86_64__
void log_marker_port_e9(const char* s);
#else
static inline void log_marker_port_e9(const char* s){ (void)s; }
#endif

#ifdef __aarch64__
#ifndef PL011_BASE
#define PL011_BASE 0x09000000u
#endif
void log_marker_pl011(const char* s);
#else
static inline void log_marker_pl011(const char* s){ (void)s; }
#endif
