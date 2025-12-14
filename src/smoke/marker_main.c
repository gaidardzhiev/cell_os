/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/log_marker.h"

void marker_main(void){
#if defined(__x86_64__)
	log_marker_port_e9("#E1 NCI ok (x86 • port 0xE9 test)\n");
#elif defined(__aarch64__)
	log_marker_pl011("#E1 NCI ok (arm64 • PL011 test)\n");
#else
	(void)0;
#endif
	for (;;) {
#if defined(__x86_64__)
		__asm__ volatile ("hlt");
#elif defined(__aarch64__)
		__asm__ volatile ("wfi");
#else
		;
#endif
	}
}
