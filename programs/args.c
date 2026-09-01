/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	char *name;
	int value;
	if (argc != 3) {
		puts("usage: args NAME NUMBER");
		return 2;
	}
	name = argv[1];
	value = atoi(argv[2]);
	puts(name);
	if (strcmp(name, "cell") == 0) {
		puts("name=cell");
	}
	if (strlen(name) > 0) {
		putchar('>');
		putchar('\n');
	}
	return value;
}
