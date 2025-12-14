# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

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
