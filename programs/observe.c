/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <cell.h>

int main(void)
{
	puts("C observation:");
	cell_capability("system.status");
	cell_capability("memory.status");
	return 0;
}
