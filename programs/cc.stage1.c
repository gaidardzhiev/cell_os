/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
char *src;
char *tx;
char *nm;
char *code;
char *dat;
char *img;
char *ws;
int sn;
int sp;
int tk;
int tv;
int cn;
int dn;
int fnc;
int gc;
int lc;
int pcnt;
int pargc;
int local_next;
int frame_pc;
int rmask;
int main_entry;
int main_argc;
int ldepth;
int ndepth;

int get32(char *p, int o) {
	int v;
	v = p[o];
	v = v | (p[o + 1] << 8);
	v = v | (p[o + 2] << 16);
	v = v | (p[o + 3] << 24);
	return v;
}

int put32(char *p, int o, int v) {
	p[o] = v & 255;
	p[o + 1] = (v >> 8) & 255;
	p[o + 2] = (v >> 16) & 255;
	p[o + 3] = (v >> 24) & 255;
	return 1;
}

int copytx(char *p) {
	int i;
	i = 0;
	while (tx[i] != 0) {
		p[i] = tx[i];
		i = i + 1;
		if (i >= 31) return 0;
	}
	p[i] = 0;
	return 1;
}

int nameeq(char *p, int n, char *s) {
	return strcmp(p + n * 32, s) == 0;
}

int nexttok() {
	int again;
	int c;
	int i;
	again = 1;
	while (again) {
		again = 0;
		while (sp < sn) {
			c = src[sp];
			if (c == 32) sp = sp + 1;
			else if (c == 9) sp = sp + 1;
			else if (c == 10) sp = sp + 1;
			else if (c == 13) sp = sp + 1;
			else c = -1;
			if (c == -1) sp = sp;
			else again = again;
			if (c == -1) break;
		}
		if (sp < sn) {
			if (src[sp] == 35) {
				while (sp < sn) {
					if (src[sp] == 10) break;
					sp = sp + 1;
				}
				again = 1;
			} else if (src[sp] == 47) {
				if (sp + 1 < sn) {
					if (src[sp + 1] == 47) {
						sp = sp + 2;
						while (sp < sn) {
							if (src[sp] == 10) break;
							sp = sp + 1;
						}
						again = 1;
					} else if (src[sp + 1] == 42) {
						sp = sp + 2;
						while (sp + 1 < sn) {
							if (src[sp] == 42) {
								if (src[sp + 1] == 47) break;
							}
							sp = sp + 1;
						}
						if (sp + 1 < sn) sp = sp + 2;
						again = 1;
					}
				}
			}
		}
	}
	if (sp >= sn) {
		tk = 0;
		return 1;
	}
	c = src[sp];
	if ((c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95) {
		i = 0;
		while (sp < sn) {
			c = src[sp];
			if ((c >= 65 && c <= 90) || (c >= 97 && c <= 122) || (c >= 48 && c <= 57) || c == 95) {
				if (i < 63) tx[i] = c;
				i = i + 1;
				sp = sp + 1;
			} else c = -1;
			if (c == -1) break;
		}
		if (i > 63) return 0;
		tx[i] = 0;
		tk = 256;
		return 1;
	}
	if (c >= 48 && c <= 57) {
		tv = 0;
		while (sp < sn) {
			c = src[sp];
			if (c >= 48 && c <= 57) {
				tv = tv * 10 + c - 48;
				sp = sp + 1;
			} else c = -1;
			if (c == -1) break;
		}
		tk = 257;
		return 1;
	}
	if (c == 34) {
		sp = sp + 1;
		i = 0;
		while (sp < sn) {
			c = src[sp];
			if (c == 34) break;
			sp = sp + 1;
			if (c == 92) {
				if (sp >= sn) return 0;
				c = src[sp];
				sp = sp + 1;
				if (c == 110) c = 10;
				else if (c == 114) c = 13;
				else if (c == 116) c = 9;
				else if (c == 48) c = 0;
			}
			if (i >= 510) return 0;
			tx[i] = c;
			i = i + 1;
		}
		if (sp >= sn) return 0;
		sp = sp + 1;
		tx[i] = 0;
		tk = 258;
		return 1;
	}
	if (sp + 1 < sn) {
		if (c == 61 && src[sp + 1] == 61) { sp = sp + 2; tk = 259; return 1; }
		if (c == 33 && src[sp + 1] == 61) { sp = sp + 2; tk = 260; return 1; }
		if (c == 60 && src[sp + 1] == 61) { sp = sp + 2; tk = 261; return 1; }
		if (c == 62 && src[sp + 1] == 61) { sp = sp + 2; tk = 262; return 1; }
		if (c == 60 && src[sp + 1] == 60) { sp = sp + 2; tk = 263; return 1; }
		if (c == 62 && src[sp + 1] == 62) { sp = sp + 2; tk = 264; return 1; }
		if (c == 38 && src[sp + 1] == 38) { sp = sp + 2; tk = 265; return 1; }
		if (c == 124 && src[sp + 1] == 124) { sp = sp + 2; tk = 266; return 1; }
	}
	sp = sp + 1;
	tk = c;
	return 1;
}

int iskw(char *s) {
	if (tk != 256) return 0;
	return strcmp(tx, s) == 0;
}

int need(int k) {
	if (tk != k) return 0;
	return nexttok();
}

int emit(int op, int d, int a, int b, int im) {
	int o;
	int pc;
	if (cn >= 15000) return -1;
	pc = cn;
	o = cn * 8;
	code[o] = op;
	code[o + 1] = d;
	code[o + 2] = a;
	code[o + 3] = b;
	put32(code, o + 4, im);
	cn = cn + 1;
	return pc;
}

int setimm(int pc, int v) {
	return put32(code, pc * 8 + 4, v);
}

int patchrel(int pc, int target) {
	return setimm(pc, target - pc - 1);
}

int ar() {
	int i;
	int bit;
	i = 0;
	while (i < 16) {
		bit = 1 << i;
		if ((rmask & bit) == 0) {
			rmask = rmask | bit;
			return i;
		}
		i = i + 1;
	}
	return -1;
}

int fr(int r) {
	int bit;
	if (r < 0) return 1;
	if (r > 15) return 1;
	bit = 1 << r;
	if ((rmask & bit) != 0) rmask = rmask ^ bit;
	return 1;
}

int addstr(char *s) {
	int off;
	int i;
	off = dn;
	i = 0;
	while (s[i] != 0) {
		if (dn >= 8191) return -1;
		dat[dn] = s[i];
		dn = dn + 1;
		i = i + 1;
	}
	if (dn >= 8192) return -1;
	dat[dn] = 0;
	dn = dn + 1;
	return off;
}

int findglobal(char *s) {
	int i;
	i = 0;
	while (i < gc) {
		if (nameeq(ws + 4352, i, s)) return i;
		i = i + 1;
	}
	return -1;
}

int addglobal(char *s, int t) {
	int i;
	int off;
	if (gc >= 64) return -1;
	i = 0;
	while (s[i] != 0) {
		(ws + 4352)[gc * 32 + i] = s[i];
		i = i + 1;
	}
	(ws + 4352)[gc * 32 + i] = 0;
	(ws + 6400)[gc] = t;
	off = gc * 8;
	put32(ws + 6464, gc * 4, off);
	gc = gc + 1;
	return gc - 1;
}

int findlocal(char *s) {
	int i;
	i = lc - 1;
	while (i >= 0) {
		if (nameeq(ws + 6912, i, s)) return i;
		i = i - 1;
	}
	return -1;
}

int addlocal(char *s, int t) {
	int i;
	if (lc >= 64) return -1;
	i = 0;
	while (s[i] != 0) {
		(ws + 6912)[lc * 32 + i] = s[i];
		i = i + 1;
	}
	(ws + 6912)[lc * 32 + i] = 0;
	(ws + 8960)[lc] = t;
	put32(ws + 9024, lc * 4, local_next);
	local_next = local_next + 8;
	lc = lc + 1;
	return lc - 1;
}

int findfunc(char *s) {
	int i;
	i = 0;
	while (i < fnc) {
		if (nameeq(ws + 1024, i, s)) return i;
		i = i + 1;
	}
	return -1;
}

int ensurefunc(char *s, int ac) {
	int i;
	int n;
	i = findfunc(s);
	if (i >= 0) return i;
	if (fnc >= 64) return -1;
	n = 0;
	while (s[n] != 0) {
		(ws + 1024)[fnc * 32 + n] = s[n];
		n = n + 1;
	}
	(ws + 1024)[fnc * 32 + n] = 0;
	put32(ws + 3072, fnc * 4, 0);
	(ws + 3328)[fnc] = ac;
	(ws + 3392)[fnc] = 0;
	fnc = fnc + 1;
	return fnc - 1;
}

int ptype() {
	int base;
	int stars;
	if (iskw("int")) base = 0;
	else if (iskw("char")) base = 3;
	else return -1;
	if (!nexttok()) return -1;
	stars = 0;
	while (tk == 42) {
		stars = stars + 1;
		if (!nexttok()) return -1;
	}
	if (base == 0) {
		if (stars == 0) return 0;
		return 1;
	}
	if (stars == 0) return 3;
	if (stars == 1) return 1;
	return 2;
}

int mk(int r, int t, int l) {
	return r | (t << 8) | (l << 16);
}

int er(int e) {
	return e & 255;
}

int et(int e) {
	return (e >> 8) & 255;
}

int el(int e) {
	return (e >> 16) & 1;
}

int rv(int e) {
	int a;
	int r;
	int t;
	if (e < 0) return -1;
	if (el(e) == 0) return e;
	a = er(e);
	t = et(e);
	r = ar();
	if (r < 0) return -1;
	if (t == 3) {
		if (emit(12, r, a, 0, 0) < 0) return -1;
	} else {
		if (emit(23, r, a, 0, 0) < 0) return -1;
	}
	fr(a);
	return mk(r, t, 0);
}

int bid(char *s) {
	if (strcmp(s, "open") == 0) return 1;
	if (strcmp(s, "close") == 0) return 2;
	if (strcmp(s, "read") == 0) return 3;
	if (strcmp(s, "write") == 0) return 4;
	if (strcmp(s, "malloc") == 0) return 7;
	if (strcmp(s, "free") == 0) return 8;
	if (strcmp(s, "install_exec") == 0) return 10;
	if (strcmp(s, "strlen") == 0) return 101;
	if (strcmp(s, "strcmp") == 0) return 102;
	if (strcmp(s, "puts") == 0) return 103;
	if (strcmp(s, "putchar") == 0) return 104;
	return 0;
}


int call(char *name) {
	int ac;
	int e;
	int i;
	int r;
	int b;
	int f;
	int pc;
	int target;
	int packed;
	int abase;
	abase = 12000 + (ndepth - 1) * 32;
	if (!need(40)) return -1;
	ac = 0;
	if (tk != 41) {
		while (1) {
			if (ac >= 8) return -1;
			e = expr();
			e = rv(e);
			if (e < 0) return -1;
			put32(ws + abase, ac * 4, e);
			ac = ac + 1;
			if (tk != 44) break;
			if (!nexttok()) return -1;
		}
	}
	if (!need(41)) return -1;
	b = bid(name);
	if (b != 0) {
		if (b == 101) {
			if (ac != 1) return -1;
			e = get32(ws + abase, 0);
			r = er(e);
			if (emit(26, r, r, 0, 0) < 0) return -1;
			return mk(r, 0, 0);
		}
		if (b == 102) {
			if (ac != 2) return -1;
			e = get32(ws + abase, 0);
			r = er(e);
			i = er(get32(ws + abase, 4));
			if (emit(43, r, r, i, 0) < 0) return -1;
			fr(i);
			return mk(r, 0, 0);
		}
		if (b == 103) {
			if (ac != 1) return -1;
			e = get32(ws + abase, 0);
			r = er(e);
			if (emit(24, 0, r, 0, 0) < 0) return -1;
			if (emit(1, r, 0, 0, 0) < 0) return -1;
			return mk(r, 0, 0);
		}
		if (b == 104) {
			if (ac != 1) return -1;
			e = get32(ws + abase, 0);
			r = er(e);
			if (emit(25, 0, r, 0, 0) < 0) return -1;
			return mk(r, 0, 0);
		}
		if (b == 2 || b == 7 || b == 8) {
			if (ac != 1) return -1;
			e = get32(ws + abase, 0);
			r = er(e);
			if (emit(29, r, r, 0, b) < 0) return -1;
			if (b == 7) return mk(r, 1, 0);
			return mk(r, 0, 0);
		}
		if (b == 1 || b == 10) {
			if (ac != 2) return -1;
			e = get32(ws + abase, 0);
			r = er(e);
			i = er(get32(ws + abase, 4));
			if (emit(29, r, r, i, b) < 0) return -1;
			fr(i);
			return mk(r, 0, 0);
		}
		if (b == 3 || b == 4) {
			if (ac != 3) return -1;
			e = get32(ws + abase, 0);
			r = er(e);
			i = er(get32(ws + abase, 4));
			pc = er(get32(ws + abase, 8));
			if (emit(29, r, r, i, b | (pc << 8)) < 0) return -1;
			fr(i);
			fr(pc);
			return mk(r, 0, 0);
		}
		return -1;
	}
	f = ensurefunc(name, ac);
	if (f < 0) return -1;
	i = 0;
	while (i < ac) {
		e = get32(ws + abase, i * 4);
		r = ar();
		if (r < 0) return -1;
		if (emit(37, r, 0, 0, i * 8) < 0) return -1;
		if (emit(33, 0, r, er(e), 0) < 0) return -1;
		fr(r);
		fr(er(e));
		i = i + 1;
	}
	r = ar();
	if (r < 0) return -1;
	target = get32(ws + 3072, f * 4);
	if (target != 0) target = target - 1;
	packed = target | (ac << 24);
	if (ac != 0) {
		i = ar();
		if (i < 0) return -1;
		if (emit(37, i, 0, 0, 0) < 0) return -1;
		pc = emit(38, r, i, 0, packed);
		fr(i);
	} else pc = emit(38, r, 0, 0, packed);
	if (pc < 0) return -1;
	if (target == 0) {
		if (pcnt >= 128) return -1;
		put32(ws + 3456, pcnt * 4, pc);
		(ws + 3968)[pcnt] = f;
		(ws + 4096)[pcnt] = ac;
		pcnt = pcnt + 1;
	}
	return mk(r, 0, 0);
}

int primary() {
	int r;
	int off;
	int i;
	int t;
	int e;
	char *name;
	if (tk == 257) {
		r = ar();
		if (r < 0) return -1;
		if (emit(1, r, 0, 0, tv) < 0) return -1;
		if (!nexttok()) return -1;
		return mk(r, 0, 0);
	}
	if (tk == 258) {
		off = addstr(tx);
		if (off < 0) return -1;
		r = ar();
		if (r < 0) return -1;
		if (emit(1, r, 0, 0, 1073741824 + off) < 0) return -1;
		if (!nexttok()) return -1;
		return mk(r, 1, 0);
	}
	if (tk == 40) {
		if (!nexttok()) return -1;
		e = expr();
		if (e < 0) return -1;
		if (!need(41)) return -1;
		return e;
	}
	if (tk != 256) return -1;
	if (ndepth >= 16) return -1;
	name = ws + 11000 + ndepth * 32;
	if (!copytx(name)) return -1;
	ndepth = ndepth + 1;
	if (!nexttok()) return -1;
	if (tk == 40) {
		e = call(name);
		ndepth = ndepth - 1;
		return e;
	}
	i = findlocal(name);
	if (i >= 0) {
		r = ar();
		if (r < 0) return -1;
		if (emit(37, r, 0, 0, get32(ws + 9024, i * 4)) < 0) return -1;
		t = (ws + 8960)[i];
		ndepth = ndepth - 1;
		return mk(r, t, 1);
	}
	i = findglobal(name);
	if (i >= 0) {
		r = ar();
		if (r < 0) return -1;
		off = 258048 + get32(ws + 6464, i * 4);
		if (emit(1, r, 0, 0, off) < 0) return -1;
		t = (ws + 6400)[i];
		ndepth = ndepth - 1;
		return mk(r, t, 1);
	}
	ndepth = ndepth - 1;
	return -1;
}

int postfix() {
	int e;
	int ix;
	int r;
	int ri;
	int sc;
	int t;
	e = primary();
	if (e < 0) return -1;
	while (tk == 91) {
		e = rv(e);
		if (e < 0) return -1;
		t = et(e);
		if (t != 1 && t != 2) return -1;
		if (!nexttok()) return -1;
		ix = expr();
		ix = rv(ix);
		if (ix < 0) return -1;
		if (!need(93)) return -1;
		r = er(e);
		ri = er(ix);
		if (t == 2) {
			sc = ar();
			if (sc < 0) return -1;
			if (emit(1, sc, 0, 0, 8) < 0) return -1;
			if (emit(6, ri, ri, sc, 0) < 0) return -1;
			fr(sc);
		}
		if (emit(3, r, r, ri, 0) < 0) return -1;
		fr(ri);
		if (t == 2) t = 1;
		else t = 3;
		e = mk(r, t, 1);
	}
	return e;
}

int unary() {
	int e;
	int r;
	int z;
	if (tk == 33) {
		if (!nexttok()) return -1;
		e = unary();
		e = rv(e);
		if (e < 0) return -1;
		r = er(e);
		z = ar();
		if (z < 0) return -1;
		if (emit(1, z, 0, 0, 0) < 0) return -1;
		if (emit(16, r, r, z, 0) < 0) return -1;
		fr(z);
		return mk(r, 0, 0);
	}
	if (tk == 45) {
		if (!nexttok()) return -1;
		e = unary();
		e = rv(e);
		if (e < 0) return -1;
		r = er(e);
		z = ar();
		if (z < 0) return -1;
		if (emit(1, z, 0, 0, 0) < 0) return -1;
		if (emit(5, r, z, r, 0) < 0) return -1;
		fr(z);
		return mk(r, 0, 0);
	}
	return postfix();
}

int mul() {
	int e;
	int q;
	int op;
	int r;
	int s;
	e = unary();
	while (tk == 42 || tk == 47 || tk == 37) {
		op = tk;
		if (!nexttok()) return -1;
		q = unary();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (op == 42) op = 6;
		else if (op == 47) op = 14;
		else op = 15;
		if (emit(op, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int add() {
	int e;
	int q;
	int op;
	int r;
	int s;
	int t;
	e = mul();
	while (tk == 43 || tk == 45) {
		op = tk;
		if (!nexttok()) return -1;
		q = mul();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		t = et(e);
		if (op == 43) op = 3;
		else op = 5;
		if (emit(op, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, t, 0);
	}
	return e;
}

int shiftx() {
	int e;
	int q;
	int op;
	int r;
	int s;
	e = add();
	while (tk == 263 || tk == 264) {
		op = tk;
		if (!nexttok()) return -1;
		q = add();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (op == 263) op = 41;
		else op = 42;
		if (emit(op, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int rel() {
	int e;
	int q;
	int op;
	int r;
	int s;
	e = shiftx();
	while (tk == 60 || tk == 62 || tk == 261 || tk == 262) {
		op = tk;
		if (!nexttok()) return -1;
		q = shiftx();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (op == 60) op = 18;
		else if (op == 62) op = 20;
		else if (op == 261) op = 19;
		else op = 21;
		if (emit(op, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int eqexpr() {
	int e;
	int q;
	int op;
	int r;
	int s;
	e = rel();
	while (tk == 259 || tk == 260) {
		op = tk;
		if (!nexttok()) return -1;
		q = rel();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (op == 259) op = 16;
		else op = 17;
		if (emit(op, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int band() {
	int e;
	int q;
	int r;
	int s;
	e = eqexpr();
	while (tk == 38) {
		if (!nexttok()) return -1;
		q = eqexpr();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (emit(39, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int bxor() {
	int e;
	int q;
	int r;
	int s;
	e = band();
	while (tk == 94) {
		if (!nexttok()) return -1;
		q = band();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (emit(40, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int bor() {
	int e;
	int q;
	int r;
	int s;
	e = bxor();
	while (tk == 124) {
		if (!nexttok()) return -1;
		q = bxor();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (emit(32, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int land() {
	int e;
	int q;
	int r;
	int s;
	e = bor();
	while (tk == 265) {
		if (!nexttok()) return -1;
		q = bor();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (emit(39, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int lor() {
	int e;
	int q;
	int r;
	int s;
	e = land();
	while (tk == 266) {
		if (!nexttok()) return -1;
		q = land();
		e = rv(e);
		q = rv(q);
		if (e < 0 || q < 0) return -1;
		r = er(e);
		s = er(q);
		if (emit(32, r, r, s, 0) < 0) return -1;
		fr(s);
		e = mk(r, 0, 0);
	}
	return e;
}

int assign() {
	int e;
	int q;
	int a;
	int r;
	int t;
	e = lor();
	if (e < 0) return -1;
	if (tk != 61) return e;
	if (el(e) == 0) return -1;
	a = er(e);
	t = et(e);
	if (!nexttok()) return -1;
	q = assign();
	q = rv(q);
	if (q < 0) return -1;
	r = er(q);
	if (t == 3) {
		if (emit(13, 0, a, r, 0) < 0) return -1;
	} else {
		if (emit(33, 0, a, r, 0) < 0) return -1;
	}
	fr(a);
	return mk(r, t, 0);
}

int expr() {
	return assign();
}

int block() {
	if (!need(123)) return 0;
	while (tk != 125) {
		if (tk == 0) return 0;
		if (!stmt()) return 0;
	}
	return nexttok();
}

int stmt() {
	int e;
	int r;
	int pc;
	int jp;
	int start;
	int t;
	int li;
	if (tk == 123) return block();
	if (iskw("if")) {
		if (!nexttok()) return 0;
		if (!need(40)) return 0;
		e = expr();
		e = rv(e);
		if (e < 0) return 0;
		if (!need(41)) return 0;
		pc = emit(7, 0, er(e), 0, 0);
		fr(er(e));
		if (pc < 0) return 0;
		if (!stmt()) return 0;
		if (iskw("else")) {
			jp = emit(9, 0, 0, 0, 0);
			if (jp < 0) return 0;
			patchrel(pc, cn);
			if (!nexttok()) return 0;
			if (!stmt()) return 0;
			patchrel(jp, cn);
		} else patchrel(pc, cn);
		return 1;
	}
	if (iskw("while")) {
		if (!nexttok()) return 0;
		if (!need(40)) return 0;
		start = cn;
		e = expr();
		e = rv(e);
		if (e < 0) return 0;
		if (!need(41)) return 0;
		pc = emit(7, 0, er(e), 0, 0);
		fr(er(e));
		if (pc < 0) return 0;
		if (ldepth >= 8) return 0;
		(ws + 10256)[ldepth] = 0;
		ldepth = ldepth + 1;
		if (!stmt()) return 0;
		e = start - cn - 1;
		jp = emit(9, 0, 0, 0, e);
		if (jp < 0) return 0;
		patchrel(pc, cn);
		ldepth = ldepth - 1;
		r = 0;
		while (r < (ws + 10256)[ldepth]) {
			e = get32(ws + 10000, ldepth * 32 + r * 4);
			patchrel(e, cn);
			r = r + 1;
		}
		return 1;
	}
	if (iskw("break")) {
		if (ldepth <= 0) return 0;
		pc = (ws + 10256)[ldepth - 1];
		if (pc >= 8) return 0;
		jp = emit(9, 0, 0, 0, 0);
		if (jp < 0) return 0;
		put32(ws + 10000, (ldepth - 1) * 32 + pc * 4, jp);
		(ws + 10256)[ldepth - 1] = pc + 1;
		if (!nexttok()) return 0;
		return need(59);
	}
	if (iskw("return")) {
		if (!nexttok()) return 0;
		e = expr();
		e = rv(e);
		if (e < 0) return 0;
		if (!need(59)) return 0;
		if (emit(31, 0, er(e), 0, 0) < 0) return 0;
		fr(er(e));
		return 1;
	}
	if (iskw("int") || iskw("char")) {
		t = ptype();
		if (t < 0) return 0;
		if (tk != 256) return 0;
		if (!copytx(nm)) return 0;
		li = addlocal(nm, t);
		if (li < 0) return 0;
		if (!nexttok()) return 0;
		if (tk == 61) {
			r = ar();
			if (r < 0) return 0;
			if (emit(37, r, 0, 0, get32(ws + 9024, li * 4)) < 0) return 0;
			if (!nexttok()) return 0;
			e = expr();
			e = rv(e);
			if (e < 0) return 0;
			if (t == 3) {
				if (emit(13, 0, r, er(e), 0) < 0) return 0;
			} else {
				if (emit(33, 0, r, er(e), 0) < 0) return 0;
			}
			fr(r);
			fr(er(e));
		}
		return need(59);
	}
	e = expr();
	e = rv(e);
	if (e < 0) return 0;
	fr(er(e));
	return need(59);
}

int external() {
	int t;
	int f;
	int i;
	int ac;
	int r;
	int li;
	int pc;
	t = ptype();
	if (t < 0) return 0;
	if (tk != 256) return 0;
	if (!copytx(nm)) return 0;
	if (!nexttok()) return 0;
	if (tk != 40) {
		if (addglobal(nm, t) < 0) return 0;
		return need(59);
	}
	if (!nexttok()) return 0;
	ac = 0;
	if (iskw("void")) {
		if (!nexttok()) return 0;
		if (tk != 41) return 0;
	} else {
		while (tk != 41) {
			if (ac >= 8) return 0;
			t = ptype();
			if (t < 0) return 0;
			if (tk != 256) return 0;
			i = 0;
			while (tx[i] != 0) {
				(ws + 9472)[ac * 32 + i] = tx[i];
				i = i + 1;
			}
			(ws + 9472)[ac * 32 + i] = 0;
			(ws + 9728)[ac] = t;
			ac = ac + 1;
			if (!nexttok()) return 0;
			if (tk != 44) break;
			if (!nexttok()) return 0;
		}
	}
	if (!need(41)) return 0;
	f = ensurefunc(nm, ac);
	if (f < 0) return 0;
	put32(ws + 3072, f * 4, cn + 1);
	(ws + 3328)[f] = ac;
	(ws + 3392)[f] = 1;
	if (strcmp(nm, "main") == 0) {
		main_entry = cn;
		main_argc = ac;
	}
	lc = 0;
	local_next = 64;
	rmask = 0;
	frame_pc = emit(36, 0, 0, 0, 0);
	if (frame_pc < 0) return 0;
	i = 0;
	while (i < ac) {
		li = addlocal(ws + 9472 + i * 32, (ws + 9728)[i]);
		if (li < 0) return 0;
		r = 15;
		if (emit(37, r, 0, 0, get32(ws + 9024, li * 4)) < 0) return 0;
		if (emit(33, 0, r, i, 0) < 0) return 0;
		i = i + 1;
	}
	if (!block()) return 0;
	r = ar();
	if (r < 0) return 0;
	if (emit(1, r, 0, 0, 0) < 0) return 0;
	if (emit(31, 0, r, 0, 0) < 0) return 0;
	fr(r);
	pc = local_next;
	pc = (pc + 7) & 2147483640;
	setimm(frame_pc, pc);
	return 1;
}

int parseall() {
	int i;
	int pc;
	int f;
	int ac;
	if (!nexttok()) return 0;
	while (tk != 0) {
		if (!external()) return 0;
	}
	if (main_entry < 0) return 0;
	i = 0;
	while (i < pcnt) {
		pc = get32(ws + 3456, i * 4);
		f = (ws + 3968)[i];
		ac = (ws + 4096)[i];
		if (get32(ws + 3072, f * 4) == 0) return 0;
		setimm(pc, get32(ws + 3072, f * 4) - 1 | (ac << 24));
		i = i + 1;
	}
	main_entry = get32(ws + 3072, findfunc("main") * 4) - 1;
	if (main_argc == 2) {
		if (emit(30, 8, 0, 1, main_entry | (2 << 24)) < 0) return 0;
	} else {
		if (emit(30, 8, 0, 0, main_entry) < 0) return 0;
	}
	if (emit(22, 0, 8, 0, 0) < 0) return 0;
	main_entry = cn - 2;
	return 1;
}

int buildimage() {
	int cb;
	int total;
	int i;
	cb = cn * 8;
	total = 64 + cb + dn;
	if (total > 131072) return -1;
	i = 0;
	while (i < 64) {
		img[i] = 0;
		i = i + 1;
	}
	put32(img, 0, 827868483);
	img[4] = 1;
	img[5] = 0;
	img[6] = 64;
	img[7] = 0;
	put32(img, 8, 8);
	put32(img, 12, cb);
	put32(img, 16, dn);
	put32(img, 20, main_entry);
	put32(img, 24, 0);
	put32(img, 28, 0);
	put32(img, 32, 262144);
	put32(img, 36, cn * 4096 + 4096);
	put32(img, 40, 1);
	put32(img, 44, 0);
	put32(img, 48, total);
	put32(img, 52, 4096);
	put32(img, 56, 0);
	put32(img, 60, 0);
	i = 0;
	while (i < dn) {
		img[64 + cb + i] = dat[i];
		i = i + 1;
	}
	return total;
}

int initwork() {
	tx = ws;
	nm = ws + 512;
	fnc = 0;
	gc = 0;
	lc = 0;
	pcnt = 0;
	pargc = 0;
	cn = 0;
	dn = 0;
	rmask = 0;
	main_entry = -1;
	main_argc = 0;
	ldepth = 0;
	ndepth = 0;
	return 1;
}

int comparefiles(char *a, char *b) {
	char *x;
	char *y;
	int fa;
	int fb;
	int na;
	int nb;
	int i;
	x = malloc(512);
	y = malloc(512);
	if (x == 0) return 2;
	if (y == 0) return 2;
	fa = open(a, 0);
	if (fa < 0) return 2;
	fb = open(b, 0);
	if (fb < 0) return 2;
	while (1) {
		na = read(fa, x, 512);
		nb = read(fb, y, 512);
		if (na != nb) return 1;
		if (na <= 0) return 0;
		i = 0;
		while (i < na) {
			if (x[i] != y[i]) return 1;
			i = i + 1;
		}
	}
	return 1;
}

int main(int argc, char **argv) {
	char *input;
	char *output;
	char *candidate;
	int emit_only;
	int i;
	int fd;
	int total;
	int rc;
	if (argc >= 2) {
		if (strcmp(argv[1], "--install") == 0) {
			if (argc != 4) return 2;
			return install_exec(argv[2], argv[3]);
		}
		if (strcmp(argv[1], "--compare") == 0) {
			if (argc != 4) return 2;
			return comparefiles(argv[2], argv[3]);
		}
	}
	input = 0;
	output = "/programs/a.out";
	emit_only = 0;
	i = 1;
	if (argc >= 2) {
		if (strcmp(argv[1], "--emit") == 0) {
			emit_only = 1;
			i = 2;
		}
	}
	while (i < argc) {
		if (strcmp(argv[i], "-o") == 0) {
			i = i + 1;
			if (i >= argc) return 2;
			output = argv[i];
		} else {
			if (input != 0) return 2;
			input = argv[i];
		}
		i = i + 1;
	}
	if (input == 0) return 2;
	src = malloc(32768);
	img = malloc(131072);
	dat = malloc(8192);
	ws = malloc(16384);
	if (src == 0) return 3;
	if (img == 0) return 3;
	if (dat == 0) return 3;
	if (ws == 0) return 3;
	code = img + 64;
	initwork();
	fd = open(input, 0);
	if (fd < 0) return 4;
	sn = read(fd, src, 32767);
	close(fd);
	if (sn <= 0) return 4;
	if (sn >= 32767) return 4;
	src[sn] = 0;
	sp = 0;
	if (!parseall()) {
		puts("cc.stage1: unsupported bootstrap C source");
		return 5;
	}
	total = buildimage();
	if (total <= 0) return 6;
	if (emit_only != 0) candidate = output;
	else candidate = "/home/.cc.bootstrap.cellx";
	fd = open(candidate, 577);
	if (fd < 0) return 7;
	rc = write(fd, img, total);
	if (rc != total) return 7;
	if (close(fd) != 0) return 7;
	if (emit_only != 0) return 0;
	return install_exec(candidate, output);
}
