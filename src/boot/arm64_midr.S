# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#

.text
.globl cgf_read_midr_el1


cgf_read_midr_el1:
	mrs x0, MIDR_EL1
	ret
