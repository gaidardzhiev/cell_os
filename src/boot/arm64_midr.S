# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

.text
.globl cgf_read_midr_el1


cgf_read_midr_el1:
	mrs x0, MIDR_EL1
	ret
