/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *src;
char *code;
char *data;
char *image;
int sn;
int sp;
int cn;
int dn;

int space(int c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void skip(void) {
	int again;
	again = 1;
	while (again) {
		again = 0;
		while (sp < sn && space(src[sp])) sp++;
		if (sp < sn && src[sp] == '#') {
			while (sp < sn && src[sp] != '\n') sp++;
			again = 1;
		}
		if (sp + 1 < sn && src[sp] == '/' && src[sp + 1] == '/') {
			sp = sp + 2;
			while (sp < sn && src[sp] != '\n') sp++;
			again = 1;
		}
		if (sp + 1 < sn && src[sp] == '/' && src[sp + 1] == '*') {
			sp = sp + 2;
			while (sp + 1 < sn && !(src[sp] == '*' && src[sp + 1] == '/')) sp++;
			if (sp + 1 < sn) sp = sp + 2;
			again = 1;
		}
	}
}

int word(char *w) {
	int i;
	i = 0;
	while (w[i]) {
		if (sp + i >= sn || src[sp + i] != w[i]) return 0;
		i++;
	}
	return 1;
}

int take(char *w) {
	int n;
	skip();
	if (!word(w)) return 0;
	n = strlen(w);
	sp = sp + n;
	return 1;
}

int need(int c) {
	skip();
	if (sp >= sn || src[sp] != c) return 0;
	sp++;
	return 1;
}

int number(void) {
	int neg;
	int v;
	skip();
	neg = 0;
	v = 0;
	if (sp < sn && src[sp] == '-') {
		neg = 1;
		sp++;
	}
	if (sp >= sn || src[sp] < '0' || src[sp] > '9') return -2147483647;
	while (sp < sn && src[sp] >= '0' && src[sp] <= '9') {
		v = v * 10 + src[sp] - '0';
		sp++;
	}
	if (neg) return -v;
	return v;
}

void put32(char *p, int off, int v) {
	p[off] = v & 255;
	p[off + 1] = (v >> 8) & 255;
	p[off + 2] = (v >> 16) & 255;
	p[off + 3] = (v >> 24) & 255;
}

int emit(int op, int dst, int a, int b, int imm) {
	int o;
	if (cn >= 128) return 0;
	o = cn * 8;
	code[o] = op;
	code[o + 1] = dst;
	code[o + 2] = a;
	code[o + 3] = b;
	put32(code, o + 4, imm);
	cn++;
	return 1;
}

int string_data(void) {
	int off;
	int c;
	if (!need('"')) return -1;
	off = dn;
	while (sp < sn && src[sp] != '"') {
		c = src[sp++];
		if (c == '\\') {
			if (sp >= sn) return -1;
			c = src[sp++];
			if (c == 'n') c = '\n';
			else if (c == 'r') c = '\r';
			else if (c == 't') c = '\t';
			else if (c == '0') c = 0;
		}
		if (dn >= 510) return -1;
		data[dn++] = c;
	}
	if (!need('"')) return -1;
	if (dn >= 511) return -1;
	data[dn++] = '\n';
	return off;
}

int parse_main(void) {
	int off;
	int len;
	int v;
	while (sp < sn && !word("main")) sp++;
	if (sp >= sn) return 0;
	while (sp < sn && src[sp] != '{') sp++;
	if (!need('{')) return 0;
	while (sp < sn) {
		skip();
		if (sp < sn && src[sp] == '}') {
			sp++;
			return emit(0, 0, 0, 0, 0);
		}
		if (take("puts")) {
			if (!need('(')) return 0;
			off = string_data();
			if (off < 0 || !need(')') || !need(';')) return 0;
			len = dn - off;
			if (!emit(10, 0, len & 255, (len >> 8) & 255, off)) return 0;
		} else if (take("putchar")) {
			if (!need('(')) return 0;
			v = number();
			if (v == -2147483647 || !need(')') || !need(';')) return 0;
			if (!emit(1, 0, 0, 0, v) || !emit(25, 0, 0, 0, 0)) return 0;
		} else if (take("return")) {
			v = number();
			if (v == -2147483647 || !need(';')) return 0;
			if (!emit(0, 0, 0, 0, v)) return 0;
			while (sp < sn && src[sp] != '}') sp++;
			if (sp < sn) sp++;
			return 1;
		} else return 0;
	}
	return 0;
}

int build_image(void) {
	int i;
	int cb;
	int total;
	cb = cn * 8;
	total = 64 + cb + dn;
	if (total > 2048) return 0;
	for (i = 0; i < total; i++) image[i] = 0;
	put32(image, 0, 0x31584543);
	image[4] = 1;
	image[6] = 64;
	put32(image, 8, 8);
	put32(image, 12, cb);
	put32(image, 16, dn);
	put32(image, 20, 0);
	put32(image, 32, 4096);
	put32(image, 36, cn * 128 + 1024);
	put32(image, 40, 0);
	put32(image, 44, 0);
	put32(image, 48, total);
	put32(image, 52, 0);
	for (i = 0; i < cb; i++) image[64 + i] = code[i];
	for (i = 0; i < dn; i++) image[64 + cb + i] = data[i];
	return total;
}

int main(int argc, char **argv) {
	char *input;
	char *output;
	int i;
	int fd;
	int total;
	input = 0;
	output = "/programs/a.out";
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0) {
			i++;
			if (i >= argc) return 2;
			output = argv[i];
		} else if (input == 0) input = argv[i];
		else return 2;
	}
	if (input == 0) return 2;
	src = malloc(2048);
	code = malloc(1024);
	data = malloc(512);
	image = malloc(2048);
	if (src == 0 || code == 0 || data == 0 || image == 0) return 3;
	fd = open(input, O_RDONLY);
	if (fd < 0) return errno;
	sn = read(fd, src, 2047);
	close(fd);
	if (sn <= 0 || sn >= 2047) return 4;
	src[sn] = 0;
	sp = 0;
	cn = 0;
	dn = 0;
	if (!parse_main()) {
		puts("cc.stage1: unsupported bootstrap C source");
		return 5;
	}
	total = build_image();
	if (total <= 0) return 6;
	fd = open("/home/.cc.stage1.cellx", O_CREAT | O_TRUNC | O_WRONLY);
	if (fd < 0) return errno;
	if (write(fd, image, total) != total) return errno;
	if (close(fd) != 0) return errno;
	if (install_exec("/home/.cc.stage1.cellx", output) != 0) return errno;
	return 0;
}
