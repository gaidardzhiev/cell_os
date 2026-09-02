/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int dump(char *path) {
	int fd;
	int n;
	char *p;
	fd = open(path, O_RDONLY);
	if (fd < 0) return errno;
	p = malloc(256);
	if (p == 0) return errno;
	n = read(fd, p, 255);
	if (n < 0) return errno;
	if (n > 0) {
		if (write(STDOUT_FILENO, p, n) != n) return errno;
	}
	free(p);
	return close(fd);
}

int main(void) {
	int fd;
	int n;
	char *p;
	puts("process:");
	if (dump("/proc/self/status") != 0) return 10;
	puts("cpu:");
	if (dump("/sys/cpu/info") != 0) return 20;
	puts("memory:");
	if (dump("/sys/memory/info") != 0) return 30;
	p = malloc(4);
	if (p == 0) return 40;
	fd = open("/dev/zero", O_RDONLY);
	if (fd < 0) return 41;
	n = read(fd, p, 4);
	if (n != 4) return 42;
	if (p[0] != 0) return 43;
	if (p[1] != 0) return 44;
	if (lseek(fd, 0, SEEK_SET) >= 0) return 45;
	if (errno != ESPIPE) return 46;
	close(fd);
	fd = open("/dev/null", O_WRONLY);
	if (fd < 0) return 50;
	if (write(fd, "discard", 7) != 7) return 51;
	close(fd);
	fd = open("/dev/console", O_WRONLY);
	if (fd < 0) return 60;
	if (write(fd, "device console ok\n", 18) != 18) return 61;
	close(fd);
	free(p);
	return 0;
}
