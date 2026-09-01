/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
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
