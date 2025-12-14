/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include "core/log_marker.h"
#include "abi/nci.h"
#include "abi/cap.h"
#include "abi/msg.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define ABI_MESSAGE \
		"#E1 NCI ok (nci=" STR(NCI_ABI_MAJOR) "." STR(NCI_ABI_MINOR) \
		" cap=" STR(CAP_ABI_MAJOR) "." STR(CAP_ABI_MINOR) \
		" msg=" STR(MSG_ABI_MAJOR) "." STR(MSG_ABI_MINOR) ")"

static void emit_banner(void) {
#if defined(__x86_64__)
	log_marker_port_e9(ABI_MESSAGE "\n");
#elif defined(__aarch64__)
	log_marker_pl011(ABI_MESSAGE "\n");
#else
	(void)0;
#endif
}

void e1_main(void) {
	emit_banner();
	for(;;) {
#if defined(__x86_64__)
		__asm__ volatile ("hlt");
#elif defined(__aarch64__)
		__asm__ volatile ("wfi");
#else
		;
#endif
	}
}
