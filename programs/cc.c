/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	char *source = 0;
	char *output = "/programs/a.out";
	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0) {
			i++;
			if (i >= argc) {
				puts("cc: option requires an argument -- o");
				return 2;
			}
			output = argv[i];
		} else if (source == 0) {
			source = argv[i];
		} else {
			puts("cc: too many input files");
			return 2;
		}
	}
	if (source == 0) {
		puts("cc: no input files");
		return 2;
	}
	if (compile(source, output) != 0) return errno;
	return 0;
}
