# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#

.section .note.Xen, "a", @note
.align 8
.extern e3_start
.long 0x00000004
.long 0x00000008
.long 0x00000012
.asciz "Xen"
.align 8
.quad e3_start
.long 0x00000004
.long 0x00000008
.long 0x00000013
.asciz "Xen"
.align 8
.quad 0
.section .note.GNU-stack,"",@progbits
