/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/cc.h"
#include "core/capability.h"
#include "core/cellexec.h"

#define CC_IDENT_MAX 63u
#define CC_STRING_MAX 512u
#define CC_VARS 8u
#define CC_TEMP_FIRST 8u
#define CC_TEMP_LAST 15u

typedef enum {
	TOK_EOF = 0,
	TOK_IDENT = 256,
	TOK_NUMBER,
	TOK_STRING,
	TOK_EQ,
	TOK_NE,
	TOK_LE,
	TOK_GE
} token_kind_t;

typedef enum {
	VT_INT = 0,
	VT_CHAR_PTR,
	VT_CHAR_PP
} value_type_t;

typedef struct {
	int kind;
	uint32_t line;
	uint32_t column;
	int32_t number;
	char text[CC_STRING_MAX];
	uint32_t text_len;
} token_t;

typedef struct {
	const char *src;
	size_t bytes;
	size_t pos;
	uint32_t line;
	uint32_t column;
	int beginning_of_line;
	cell_cc_diag_t *diag;
} lexer_t;

typedef struct {
	char name[CC_IDENT_MAX + 1u];
	uint8_t reg;
	value_type_t type;
} variable_t;

typedef struct {
	uint8_t reg;
	value_type_t type;
} value_t;

typedef struct {
	lexer_t lex;
	token_t tok;
	cell_exec_insn_t code[CELL_CC_MAX_CODE];
	uint32_t code_count;
	uint8_t data[CELL_CC_MAX_DATA];
	uint32_t data_bytes;
	variable_t vars[CC_VARS];
	uint32_t var_count;
	uint8_t temp_next;
	uint64_t capability_mask;
	cell_cc_diag_t *diag;
	int failed;
} compiler_t;

static compiler_t g_cc;

static int s_eq(const char *a, const char *b) {
	if (!a || !b) return 0;
	while (*a && *b) if (*a++ != *b++) return 0;
	return *a == 0 && *b == 0;
}

static void s_copy(char *dst, size_t cap, const char *src) {
	if (!dst || !cap) return;
	size_t n = 0;
	while (src && src[n] && n + 1u < cap) { dst[n] = src[n]; ++n; }
	dst[n] = 0;
}

static void zero_bytes(void *p, size_t n) {
	uint8_t *b = (uint8_t *)p;
	while (n--) *b++ = 0;
}

static void diag_set(cell_cc_diag_t *d, cell_cc_status_t status,
	uint32_t line, uint32_t column, const char *message) {
	if (!d || d->status != CELL_CC_OK) return;
	d->status = status;
	d->line = line;
	d->column = column;
	s_copy(d->message, sizeof(d->message), message);
}

const char *cell_cc_status_name(cell_cc_status_t status) {
	switch (status) {
	case CELL_CC_OK: return "ok";
	case CELL_CC_BAD_ARGUMENT: return "bad_argument";
	case CELL_CC_SOURCE_TOO_LARGE: return "source_too_large";
	case CELL_CC_LEX_ERROR: return "lex_error";
	case CELL_CC_PARSE_ERROR: return "parse_error";
	case CELL_CC_TOO_MANY_VARIABLES: return "too_many_variables";
	case CELL_CC_TOO_COMPLEX: return "too_complex";
	case CELL_CC_OUTPUT_TOO_LARGE: return "output_too_large";
	case CELL_CC_UNKNOWN_CAPABILITY: return "unknown_capability";
	default: return "unknown";
	}
}

static char lx_peek(const lexer_t *l) { return l->pos < l->bytes ? l->src[l->pos] : 0; }
static char lx_peek2(const lexer_t *l) { return l->pos + 1u < l->bytes ? l->src[l->pos + 1u] : 0; }

static char lx_get(lexer_t *l) {
	if (l->pos >= l->bytes) return 0;
	char c = l->src[l->pos++];
	if (c == '\n') {
		++l->line; l->column = 1u; l->beginning_of_line = 1;
	} else {
		++l->column;
		if (c != ' ' && c != '\t' && c != '\r') l->beginning_of_line = 0;
	}
	return c;
}

static int is_alpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alnum(char c) { return is_alpha(c) || is_digit(c); }

static int skip_include(lexer_t *l) {
	uint32_t line = l->line, col = l->column;
	(void)lx_get(l);
	while (lx_peek(l) == ' ' || lx_peek(l) == '\t') (void)lx_get(l);
	char word[16]; size_t n = 0;
	while (is_alpha(lx_peek(l)) && n + 1u < sizeof(word)) word[n++] = lx_get(l);
	word[n] = 0;
	if (!s_eq(word, "include")) {
		diag_set(l->diag, CELL_CC_LEX_ERROR, line, col, "only #include is supported");
		return 0;
	}
	while (lx_peek(l) && lx_peek(l) != '\n') (void)lx_get(l);
	return 1;
}

static int skip_space_comments(lexer_t *l) {
	for (;;) {
		char c = lx_peek(l);
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { (void)lx_get(l); continue; }
		if (c == '#' && l->beginning_of_line) { if (!skip_include(l)) return 0; continue; }
		if (c == '/' && lx_peek2(l) == '/') {
			(void)lx_get(l); (void)lx_get(l);
			while (lx_peek(l) && lx_peek(l) != '\n') (void)lx_get(l);
			continue;
		}
		if (c == '/' && lx_peek2(l) == '*') {
			uint32_t line = l->line, col = l->column;
			(void)lx_get(l); (void)lx_get(l);
			while (lx_peek(l) && !(lx_peek(l) == '*' && lx_peek2(l) == '/')) (void)lx_get(l);
			if (!lx_peek(l)) { diag_set(l->diag, CELL_CC_LEX_ERROR, line, col, "unterminated comment"); return 0; }
			(void)lx_get(l); (void)lx_get(l); continue;
		}
		return 1;
	}
}

static int hex_digit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static int escaped_char(lexer_t *l, uint32_t line, uint32_t col, char *out) {
	char c = lx_get(l);
	if (!c) { diag_set(l->diag, CELL_CC_LEX_ERROR, line, col, "unterminated character escape"); return 0; }
	switch (c) {
	case 'n': *out = '\n'; return 1;
	case 'r': *out = '\r'; return 1;
	case 't': *out = '\t'; return 1;
	case '0': *out = 0; return 1;
	case '\\': *out = '\\'; return 1;
	case '\'': *out = '\''; return 1;
	case '"': *out = '"'; return 1;
	default: diag_set(l->diag, CELL_CC_LEX_ERROR, line, col, "unsupported character escape"); return 0;
	}
}

static int lex_next(lexer_t *l, token_t *t) {
	zero_bytes(t, sizeof(*t));
	if (!skip_space_comments(l)) return 0;
	t->line = l->line; t->column = l->column;
	char c = lx_peek(l);
	if (!c) { t->kind = TOK_EOF; return 1; }
	if (is_alpha(c)) {
		t->kind = TOK_IDENT;
		while (is_alnum(lx_peek(l))) {
			if (t->text_len + 1u >= CC_IDENT_MAX + 1u) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "identifier too long"); return 0; }
			t->text[t->text_len++] = lx_get(l);
		}
		t->text[t->text_len] = 0; return 1;
	}
	if (is_digit(c)) {
		int64_t value = 0; int base = 10;
		if (c == '0' && (lx_peek2(l) == 'x' || lx_peek2(l) == 'X')) {
			(void)lx_get(l); (void)lx_get(l); base = 16;
			if (hex_digit(lx_peek(l)) < 0) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "invalid hexadecimal constant"); return 0; }
			while (hex_digit(lx_peek(l)) >= 0) {
				value = value * 16 + hex_digit(lx_get(l));
				if (value > 0x7FFFFFFFll) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "integer constant out of range"); return 0; }
			}
		} else {
			while (is_digit(lx_peek(l))) {
				value = value * base + (lx_get(l) - '0');
				if (value > 0x7FFFFFFFll) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "integer constant out of range"); return 0; }
			}
		}
		t->kind = TOK_NUMBER; t->number = (int32_t)value; return 1;
	}
	if (c == '\'') {
		(void)lx_get(l);
		char v = lx_get(l);
		if (!v || v == '\n' || v == '\r') { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "invalid character constant"); return 0; }
		if (v == '\\' && !escaped_char(l, t->line, t->column, &v)) return 0;
		if (lx_get(l) != '\'') { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "character constant must contain one byte"); return 0; }
		t->kind = TOK_NUMBER; t->number = (unsigned char)v; return 1;
	}
	if (c == '"') {
		t->kind = TOK_STRING; (void)lx_get(l);
		while ((c = lx_get(l)) != 0 && c != '"') {
			if (c == '\n' || c == '\r') { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "newline in string literal"); return 0; }
			if (c == '\\' && !escaped_char(l, t->line, t->column, &c)) return 0;
			if (t->text_len + 1u >= sizeof(t->text)) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "string literal too long"); return 0; }
			t->text[t->text_len++] = c;
		}
		if (c != '"') { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "unterminated string literal"); return 0; }
		t->text[t->text_len] = 0; return 1;
	}
	if (c == '=' && lx_peek2(l) == '=') { (void)lx_get(l); (void)lx_get(l); t->kind = TOK_EQ; return 1; }
	if (c == '!' && lx_peek2(l) == '=') { (void)lx_get(l); (void)lx_get(l); t->kind = TOK_NE; return 1; }
	if (c == '<' && lx_peek2(l) == '=') { (void)lx_get(l); (void)lx_get(l); t->kind = TOK_LE; return 1; }
	if (c == '>' && lx_peek2(l) == '=') { (void)lx_get(l); (void)lx_get(l); t->kind = TOK_GE; return 1; }
	if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '=' || c == ';' || c == ',' ||
	    c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' || c == '<' || c == '>' || c == '!') {
		t->kind = (unsigned char)lx_get(l); return 1;
	}
	diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "unsupported character"); return 0;
}

static int next(compiler_t *c) {
	if (!lex_next(&c->lex, &c->tok)) { c->failed = 1; return 0; }
	return 1;
}
static int fail_at(compiler_t *c, cell_cc_status_t status, const char *msg) {
	diag_set(c->diag, status, c->tok.line, c->tok.column, msg); c->failed = 1; return 0;
}
static int tok_ident(const compiler_t *c, const char *s) { return c->tok.kind == TOK_IDENT && s_eq(c->tok.text, s); }
static int expect(compiler_t *c, int kind, const char *msg) {
	if (c->tok.kind != kind) return fail_at(c, CELL_CC_PARSE_ERROR, msg);
	return next(c);
}
static int expect_ident(compiler_t *c, const char *name, const char *msg) {
	if (!tok_ident(c, name)) return fail_at(c, CELL_CC_PARSE_ERROR, msg);
	return next(c);
}

static int emit(compiler_t *c, uint8_t opcode, uint8_t dst, uint8_t a, uint8_t b, int32_t imm) {
	if (c->code_count >= CELL_CC_MAX_CODE) return fail_at(c, CELL_CC_TOO_COMPLEX, "program has too many instructions");
	cell_exec_insn_t *in = &c->code[c->code_count++];
	in->opcode = opcode; in->dst = dst; in->a = a; in->b = b; in->imm = imm; return 1;
}
static int patch_branch(compiler_t *c, uint32_t pc, uint32_t target) {
	if (pc >= c->code_count) return 0;
	int64_t rel = (int64_t)target - ((int64_t)pc + 1);
	if (rel < -0x80000000ll || rel > 0x7FFFFFFFll) return 0;
	c->code[pc].imm = (int32_t)rel; return 1;
}
static int alloc_temp(compiler_t *c, uint8_t *r) {
	if (c->temp_next > CC_TEMP_LAST) return fail_at(c, CELL_CC_TOO_COMPLEX, "expression nesting exceeds register budget");
	*r = c->temp_next++; return 1;
}
static void free_temp(compiler_t *c, uint8_t r) {
	if (r + 1u == c->temp_next && r >= CC_TEMP_FIRST) --c->temp_next;
}

static int find_var(const compiler_t *c, const char *name, uint8_t *reg, value_type_t *type) {
	for (uint32_t i = 0; i < c->var_count; ++i) if (s_eq(c->vars[i].name, name)) {
		if (reg) *reg = c->vars[i].reg;
		if (type) *type = c->vars[i].type;
		return 1;
	}
	return 0;
}
static int add_var(compiler_t *c, const char *name, value_type_t type, uint8_t *reg) {
	if (find_var(c, name, 0, 0)) return fail_at(c, CELL_CC_PARSE_ERROR, "duplicate local variable");
	if (c->var_count >= CC_VARS) return fail_at(c, CELL_CC_TOO_MANY_VARIABLES, "too many local variables and parameters");
	variable_t *v = &c->vars[c->var_count];
	s_copy(v->name, sizeof(v->name), name); v->reg = (uint8_t)c->var_count; v->type = type;
	if (reg) *reg = v->reg;
	++c->var_count;
	return 1;
}

static int add_data(compiler_t *c, const char *s, uint32_t n, int suffix,
	uint32_t *offset, uint32_t *length) {
	uint32_t extra = suffix >= 0 ? 1u : 0u;
	if (c->data_bytes + n + extra > CELL_CC_MAX_DATA || n + extra > 65535u)
		return fail_at(c, CELL_CC_OUTPUT_TOO_LARGE, "string data exceeds compiler limit");
	*offset = c->data_bytes; if (length) *length = n + extra;
	for (uint32_t i = 0; i < n; ++i) c->data[c->data_bytes++] = (uint8_t)s[i];
	if (suffix >= 0) c->data[c->data_bytes++] = (uint8_t)suffix;
	return 1;
}

static cell_capability_id_t cap_from_name(const char *name) {
	static const struct { const char *name; cell_capability_id_t id; } table[] = {
		{"system.status", CELL_CAP_SYSTEM_STATUS}, {"cpu.info", CELL_CAP_CPU_INFO},
		{"memory.status", CELL_CAP_MEMORY_STATUS}, {"storage.list", CELL_CAP_STORAGE_LIST},
		{"storage.list_dir", CELL_CAP_STORAGE_LIST_DIR}, {"storage.pwd", CELL_CAP_STORAGE_PWD},
		{"network.list", CELL_CAP_NETWORK_LIST}, {"gpu.info", CELL_CAP_GPU_INFO},
		{"usb.list", CELL_CAP_USB_LIST}, {"display.info", CELL_CAP_DISPLAY_INFO},
		{"power.status", CELL_CAP_POWER_STATUS}
	};
	for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); ++i) if (s_eq(name, table[i].name)) return table[i].id;
	return CELL_CAP_INVALID;
}

static int parse_expr(compiler_t *c, value_t *out);

static int require_pointer(compiler_t *c, value_type_t t, const char *msg) {
	if (t != VT_CHAR_PTR) return fail_at(c, CELL_CC_PARSE_ERROR, msg);
	return 1;
}

static int parse_builtin_expr(compiler_t *c, value_t *out) {
	int which = tok_ident(c, "strlen") ? 1 : tok_ident(c, "atoi") ? 2 : tok_ident(c, "strcmp") ? 3 : 0;
	if (!which) return 0;
	if (!next(c) || !expect(c, '(', "expected '(' after library function")) return 0;
	value_t a;
	if (!parse_expr(c, &a) || !require_pointer(c, a.type, "library function requires char pointer")) return 0;
	if (which == 3) {
		if (!expect(c, ',', "expected ',' in strcmp")) return 0;
		if (c->tok.kind != TOK_STRING) return fail_at(c, CELL_CC_PARSE_ERROR, "bootstrap strcmp requires a string literal as second argument");
		uint32_t off = 0;
		if (!add_data(c, c->tok.text, c->tok.text_len, 0, &off, 0) || !next(c) || !expect(c, ')', "expected ')' after strcmp")) return 0;
		if (!emit(c, CELL_EXEC_OP_STRCMPC, a.reg, a.reg, 0, (int32_t)off)) return 0;
	} else {
		if (!expect(c, ')', "expected ')' after library function")) return 0;
		if (!emit(c, which == 1 ? CELL_EXEC_OP_STRLEN : CELL_EXEC_OP_ATOI, a.reg, a.reg, 0, 0)) return 0;
	}
	a.type = VT_INT; *out = a; return 1;
}

static int parse_primary(compiler_t *c, value_t *out) {
	if (c->tok.kind == TOK_NUMBER) {
		int32_t value = c->tok.number; uint8_t r;
		if (!alloc_temp(c, &r) || !next(c) || !emit(c, CELL_EXEC_OP_MOVI, r, 0, 0, value)) return 0;
		out->reg = r; out->type = VT_INT; return 1;
	}
	if (c->tok.kind == TOK_IDENT && (tok_ident(c, "strlen") || tok_ident(c, "atoi") || tok_ident(c, "strcmp")))
		return parse_builtin_expr(c, out);
	if (c->tok.kind == TOK_IDENT) {
		char name[CC_IDENT_MAX + 1u]; s_copy(name, sizeof(name), c->tok.text);
		uint8_t vr, tr; value_type_t type;
		if (!find_var(c, name, &vr, &type)) return fail_at(c, CELL_CC_PARSE_ERROR, "unknown local variable");
		if (!next(c) || !alloc_temp(c, &tr) || !emit(c, CELL_EXEC_OP_MOV, tr, vr, 0, 0)) return 0;
		out->reg = tr; out->type = type;
		while (c->tok.kind == '[') {
			if (out->type != VT_CHAR_PTR && out->type != VT_CHAR_PP) return fail_at(c, CELL_CC_PARSE_ERROR, "indexing requires char pointer");
			value_t index;
			if (!next(c) || !parse_expr(c, &index) || index.type != VT_INT || !expect(c, ']', "expected ']' after index")) return 0;
			if (out->type == VT_CHAR_PP) {
				uint8_t scale;
				if (!alloc_temp(c, &scale) || !emit(c, CELL_EXEC_OP_MOVI, scale, 0, 0, 8) ||
				    !emit(c, CELL_EXEC_OP_MUL, index.reg, index.reg, scale, 0)) return 0;
				free_temp(c, scale);
			}
			if (!emit(c, CELL_EXEC_OP_ADD, out->reg, out->reg, index.reg, 0)) return 0;
			free_temp(c, index.reg);
			if (out->type == VT_CHAR_PP) {
				if (!emit(c, CELL_EXEC_OP_LOAD64, out->reg, out->reg, 0, 0)) return 0;
				out->type = VT_CHAR_PTR;
			} else {
				if (!emit(c, CELL_EXEC_OP_LOAD8, out->reg, out->reg, 0, 0)) return 0;
				out->type = VT_INT;
			}
		}
		return 1;
	}
	if (c->tok.kind == '(') {
		if (!next(c) || !parse_expr(c, out) || !expect(c, ')', "expected ')'")) return 0;
		return 1;
	}
	return fail_at(c, CELL_CC_PARSE_ERROR, "expected expression");
}

static int parse_unary(compiler_t *c, value_t *out) {
	if (c->tok.kind == '-') {
		if (!next(c) || !parse_unary(c, out) || out->type != VT_INT) return fail_at(c, CELL_CC_PARSE_ERROR, "unary '-' requires integer");
		uint8_t z;
		if (!alloc_temp(c, &z) || !emit(c, CELL_EXEC_OP_MOVI, z, 0, 0, 0) || !emit(c, CELL_EXEC_OP_SUB, out->reg, z, out->reg, 0)) return 0;
		free_temp(c, z); return 1;
	}
	if (c->tok.kind == '!') {
		if (!next(c) || !parse_unary(c, out)) return 0;
		uint8_t z;
		if (!alloc_temp(c, &z) || !emit(c, CELL_EXEC_OP_MOVI, z, 0, 0, 0) || !emit(c, CELL_EXEC_OP_CMPEQ, out->reg, out->reg, z, 0)) return 0;
		free_temp(c, z); out->type = VT_INT; return 1;
	}
	if (c->tok.kind == '*') {
		if (!next(c) || !parse_unary(c, out)) return 0;
		if (out->type == VT_CHAR_PTR) {
			if (!emit(c, CELL_EXEC_OP_LOAD8, out->reg, out->reg, 0, 0)) return 0;
			out->type = VT_INT; return 1;
		}
		if (out->type == VT_CHAR_PP) {
			if (!emit(c, CELL_EXEC_OP_LOAD64, out->reg, out->reg, 0, 0)) return 0;
			out->type = VT_CHAR_PTR; return 1;
		}
		return fail_at(c, CELL_CC_PARSE_ERROR, "unary '*' requires pointer");
	}
	return parse_primary(c, out);
}

static int parse_mul(compiler_t *c, value_t *out) {
	if (!parse_unary(c, out)) return 0;
	while (c->tok.kind == '*' || c->tok.kind == '/' || c->tok.kind == '%') {
		int op = c->tok.kind; value_t right;
		if (out->type != VT_INT || !next(c) || !parse_unary(c, &right) || right.type != VT_INT)
			return fail_at(c, CELL_CC_PARSE_ERROR, "multiplicative operators require integers");
		uint8_t opcode = op == '*' ? CELL_EXEC_OP_MUL : op == '/' ? CELL_EXEC_OP_DIV : CELL_EXEC_OP_MOD;
		if (!emit(c, opcode, out->reg, out->reg, right.reg, 0)) return 0;
		free_temp(c, right.reg);
	}
	return 1;
}

static int scale_pointer_index(compiler_t *c, value_t *v, value_type_t pointer_type) {
	if (pointer_type != VT_CHAR_PP) return 1;
	uint8_t scale;
	if (!alloc_temp(c, &scale) || !emit(c, CELL_EXEC_OP_MOVI, scale, 0, 0, 8) ||
	    !emit(c, CELL_EXEC_OP_MUL, v->reg, v->reg, scale, 0)) return 0;
	free_temp(c, scale); return 1;
}

static int parse_add(compiler_t *c, value_t *out) {
	if (!parse_mul(c, out)) return 0;
	while (c->tok.kind == '+' || c->tok.kind == '-') {
		int op = c->tok.kind; value_t right;
		if (!next(c) || !parse_mul(c, &right)) return 0;
		if (out->type == VT_INT && right.type == VT_INT) {
			if (!emit(c, op == '+' ? CELL_EXEC_OP_ADD : CELL_EXEC_OP_SUB, out->reg, out->reg, right.reg, 0)) return 0;
		} else if ((out->type == VT_CHAR_PTR || out->type == VT_CHAR_PP) && right.type == VT_INT) {
			if (!scale_pointer_index(c, &right, out->type) || !emit(c, op == '+' ? CELL_EXEC_OP_ADD : CELL_EXEC_OP_SUB,
				out->reg, out->reg, right.reg, 0)) return 0;
		} else return fail_at(c, CELL_CC_PARSE_ERROR, "unsupported pointer arithmetic");
		free_temp(c, right.reg);
	}
	return 1;
}

static int comparison_opcode(int kind) {
	switch (kind) {
	case TOK_EQ: return CELL_EXEC_OP_CMPEQ;
	case TOK_NE: return CELL_EXEC_OP_CMPNE;
	case '<': return CELL_EXEC_OP_CMPLT;
	case TOK_LE: return CELL_EXEC_OP_CMPLE;
	case '>': return CELL_EXEC_OP_CMPGT;
	case TOK_GE: return CELL_EXEC_OP_CMPGE;
	default: return 0;
	}
}

static int parse_compare(compiler_t *c, value_t *out) {
	if (!parse_add(c, out)) return 0;
	while (comparison_opcode(c->tok.kind)) {
		int opcode = comparison_opcode(c->tok.kind); value_t right;
		if (!next(c) || !parse_add(c, &right)) return 0;
		if (out->type != right.type && !(out->type != VT_INT && right.type == VT_INT) && !(out->type == VT_INT && right.type != VT_INT))
			return fail_at(c, CELL_CC_PARSE_ERROR, "incompatible comparison operands");
		if (!emit(c, (uint8_t)opcode, out->reg, out->reg, right.reg, 0)) return 0;
		free_temp(c, right.reg); out->type = VT_INT;
	}
	return 1;
}

static int parse_expr(compiler_t *c, value_t *out) { return parse_compare(c, out); }

static int parse_false_branch(compiler_t *c, uint32_t *branch_pc) {
	value_t v;
	if (!parse_expr(c, &v)) return 0;
	*branch_pc = c->code_count;
	if (!emit(c, CELL_EXEC_OP_JZ, 0, v.reg, 0, 0)) return 0;
	free_temp(c, v.reg); return 1;
}

static int parse_type(compiler_t *c, value_type_t *type) {
	if (tok_ident(c, "int")) { *type = VT_INT; return next(c); }
	if (!tok_ident(c, "char")) return fail_at(c, CELL_CC_PARSE_ERROR, "expected C type");
	if (!next(c)) return 0;
	unsigned stars = 0;
	while (c->tok.kind == '*') { ++stars; if (stars > 2u || !next(c)) return stars <= 2u ? 0 : fail_at(c, CELL_CC_PARSE_ERROR, "pointer depth exceeds bootstrap C subset"); }
	*type = stars == 0u ? VT_INT : stars == 1u ? VT_CHAR_PTR : VT_CHAR_PP;
	return 1;
}

static int compatible(value_type_t dst, value_type_t src) { return dst == src; }
static int parse_statement(compiler_t *c);

static int parse_block(compiler_t *c) {
	if (!expect(c, '{', "expected '{'")) return 0;
	while (c->tok.kind != '}' && c->tok.kind != TOK_EOF) if (!parse_statement(c)) return 0;
	return expect(c, '}', "expected '}'");
}

static int parse_declaration(compiler_t *c) {
	value_type_t type;
	if (!parse_type(c, &type)) return 0;
	if (c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected variable name");
	char name[CC_IDENT_MAX + 1u]; s_copy(name, sizeof(name), c->tok.text); uint8_t vr;
	if (!next(c) || !add_var(c, name, type, &vr)) return 0;
	if (c->tok.kind == '=') {
		value_t v;
		if (!next(c) || !parse_expr(c, &v) || !compatible(type, v.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "initializer type mismatch");
		if (!emit(c, CELL_EXEC_OP_MOV, vr, v.reg, 0, 0)) return 0;
		free_temp(c, v.reg);
	} else if (!emit(c, CELL_EXEC_OP_MOVI, vr, 0, 0, 0)) return 0;
	return expect(c, ';', "expected ';' after declaration");
}

static int parse_puts(compiler_t *c) {
	if (!expect_ident(c, "puts", "expected puts") || !expect(c, '(', "expected '(' after puts")) return 0;
	if (c->tok.kind == TOK_STRING) {
		uint32_t off, len;
		if (!add_data(c, c->tok.text, c->tok.text_len, '\n', &off, &len) || !next(c) ||
		    !expect(c, ')', "expected ')' after puts") || !expect(c, ';', "expected ';' after puts")) return 0;
		return emit(c, CELL_EXEC_OP_PUTS, 0, (uint8_t)(len & 0xffu), (uint8_t)((len >> 8) & 0xffu), (int32_t)off);
	}
	value_t v;
	if (!parse_expr(c, &v) || !require_pointer(c, v.type, "puts requires string literal or char pointer") ||
	    !expect(c, ')', "expected ')' after puts") || !expect(c, ';', "expected ';' after puts")) return 0;
	int ok = emit(c, CELL_EXEC_OP_PUTSM, 0, v.reg, 0, 0); free_temp(c, v.reg); return ok;
}

static int parse_putchar(compiler_t *c) {
	if (!expect_ident(c, "putchar", "expected putchar") || !expect(c, '(', "expected '(' after putchar")) return 0;
	value_t v;
	if (!parse_expr(c, &v) || v.type != VT_INT || !expect(c, ')', "expected ')' after putchar") ||
	    !expect(c, ';', "expected ';' after putchar")) return fail_at(c, CELL_CC_PARSE_ERROR, "putchar requires integer");
	int ok = emit(c, CELL_EXEC_OP_PUTC, 0, v.reg, 0, 0); free_temp(c, v.reg); return ok;
}

static int parse_capability_call(compiler_t *c) {
	if (!expect_ident(c, "cell_capability", "expected cell_capability") || !expect(c, '(', "expected '(' after cell_capability")) return 0;
	if (c->tok.kind != TOK_STRING) return fail_at(c, CELL_CC_PARSE_ERROR, "cell_capability requires a string literal");
	cell_capability_id_t id = cap_from_name(c->tok.text);
	if (id == CELL_CAP_INVALID) return fail_at(c, CELL_CC_UNKNOWN_CAPABILITY, "unknown Cell capability");
	c->capability_mask |= 1ull << (uint32_t)id;
	if (!next(c) || !expect(c, ')', "expected ')' after cell_capability") || !expect(c, ';', "expected ';' after cell_capability")) return 0;
	return emit(c, CELL_EXEC_OP_CAP, 0, 0, 0, (int32_t)id);
}

static int parse_if(compiler_t *c) {
	if (!expect_ident(c, "if", "expected if") || !expect(c, '(', "expected '(' after if")) return 0;
	uint32_t false_pc;
	if (!parse_false_branch(c, &false_pc) || !expect(c, ')', "expected ')' after condition") || !parse_statement(c)) return 0;
	if (tok_ident(c, "else")) {
		uint32_t end_pc = c->code_count;
		if (!emit(c, CELL_EXEC_OP_JMP, 0, 0, 0, 0)) return 0;
		if (!patch_branch(c, false_pc, c->code_count) || !next(c) || !parse_statement(c)) return 0;
		return patch_branch(c, end_pc, c->code_count);
	}
	return patch_branch(c, false_pc, c->code_count);
}

static int parse_while(compiler_t *c) {
	if (!expect_ident(c, "while", "expected while") || !expect(c, '(', "expected '(' after while")) return 0;
	uint32_t top = c->code_count, false_pc;
	if (!parse_false_branch(c, &false_pc) || !expect(c, ')', "expected ')' after condition") || !parse_statement(c)) return 0;
	uint32_t back = c->code_count;
	if (!emit(c, CELL_EXEC_OP_JMP, 0, 0, 0, 0) || !patch_branch(c, back, top)) return 0;
	return patch_branch(c, false_pc, c->code_count);
}

static int parse_return(compiler_t *c) {
	if (!expect_ident(c, "return", "expected return")) return 0;
	value_t v;
	if (!parse_expr(c, &v) || v.type != VT_INT || !expect(c, ';', "expected ';' after return"))
		return fail_at(c, CELL_CC_PARSE_ERROR, "return requires integer expression");
	int ok = emit(c, CELL_EXEC_OP_EXIT, 0, v.reg, 0, 0); free_temp(c, v.reg); return ok;
}

static int parse_assignment(compiler_t *c) {
	char name[CC_IDENT_MAX + 1u]; s_copy(name, sizeof(name), c->tok.text);
	uint8_t vr; value_type_t type;
	if (!find_var(c, name, &vr, &type)) return fail_at(c, CELL_CC_PARSE_ERROR, "assignment to unknown local variable");
	if (!next(c) || !expect(c, '=', "expected '=' in assignment")) return 0;
	value_t v;
	if (!parse_expr(c, &v) || !compatible(type, v.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "assignment type mismatch");
	if (!expect(c, ';', "expected ';' after assignment") || !emit(c, CELL_EXEC_OP_MOV, vr, v.reg, 0, 0)) return 0;
	free_temp(c, v.reg); return 1;
}

static int parse_statement(compiler_t *c) {
	if (c->tok.kind == ';') return next(c);
	if (c->tok.kind == '{') return parse_block(c);
	if (tok_ident(c, "int") || tok_ident(c, "char")) return parse_declaration(c);
	if (tok_ident(c, "puts")) return parse_puts(c);
	if (tok_ident(c, "putchar")) return parse_putchar(c);
	if (tok_ident(c, "cell_capability")) return parse_capability_call(c);
	if (tok_ident(c, "if")) return parse_if(c);
	if (tok_ident(c, "while")) return parse_while(c);
	if (tok_ident(c, "return")) return parse_return(c);
	if (c->tok.kind == TOK_IDENT) return parse_assignment(c);
	return fail_at(c, CELL_CC_PARSE_ERROR, "unsupported statement in bootstrap C subset");
}

static int parse_main_parameters(compiler_t *c) {
	if (tok_ident(c, "void")) return next(c);
	if (c->tok.kind == ')') return 1;
	if (!tok_ident(c, "int")) return fail_at(c, CELL_CC_PARSE_ERROR, "expected void or int argc parameter");
	if (!next(c) || c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected argc parameter name");
	char argc_name[CC_IDENT_MAX + 1u]; s_copy(argc_name, sizeof(argc_name), c->tok.text);
	if (!next(c) || !expect(c, ',', "expected ',' between main parameters") || !tok_ident(c, "char") || !next(c) ||
	    !expect(c, '*', "expected first '*' in char **argv") || !expect(c, '*', "expected second '*' in char **argv") ||
	    c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected char **argv parameter");
	char argv_name[CC_IDENT_MAX + 1u]; s_copy(argv_name, sizeof(argv_name), c->tok.text);
	if (!next(c) || !add_var(c, argc_name, VT_INT, 0) || !add_var(c, argv_name, VT_CHAR_PP, 0)) return 0;
	return 1;
}

static int parse_translation_unit(compiler_t *c) {
	if (!expect_ident(c, "int", "expected int main") || !expect_ident(c, "main", "expected main") ||
	    !expect(c, '(', "expected '(' after main") || !parse_main_parameters(c) ||
	    !expect(c, ')', "expected ')' after main parameters") || !parse_block(c)) return 0;
	if (c->tok.kind != TOK_EOF) return fail_at(c, CELL_CC_PARSE_ERROR, "user-defined functions are not supported yet");
	return emit(c, CELL_EXEC_OP_HALT, 0, 0, 0, 0);
}

int cell_cc_compile(const char *source, size_t source_bytes,
	void *output, size_t output_cap, size_t *output_bytes, cell_cc_diag_t *diag) {
	if (output_bytes) *output_bytes = 0;
	if (diag) { zero_bytes(diag, sizeof(*diag)); diag->status = CELL_CC_OK; }
	if (!source || !output || !output_bytes || !diag) return 0;
	if (!source_bytes || source_bytes > CELL_CC_SOURCE_MAX) {
		diag_set(diag, CELL_CC_SOURCE_TOO_LARGE, 1, 1, "source file exceeds bootstrap compiler limit"); return 0;
	}
	compiler_t *c = &g_cc; zero_bytes(c, sizeof(*c)); c->diag = diag; c->temp_next = CC_TEMP_FIRST;
	c->lex.src = source; c->lex.bytes = source_bytes; c->lex.line = 1u; c->lex.column = 1u;
	c->lex.beginning_of_line = 1; c->lex.diag = diag;
	if (!next(c) || !parse_translation_unit(c) || c->failed) return 0;
	uint64_t code_bytes = (uint64_t)c->code_count * CELL_EXEC_INSN_BYTES;
	uint64_t payload = code_bytes + c->data_bytes;
	uint64_t total = CELL_EXEC_HEADER_BYTES + payload;
	if (total > output_cap || total > CELL_EXEC_FILE_MAX) {
		diag_set(diag, CELL_CC_OUTPUT_TOO_LARGE, 1, 1, "compiled program exceeds CellExec-1 file limit"); return 0;
	}
	uint8_t *dst = (uint8_t *)output; zero_bytes(dst, (size_t)total);
	cell_exec_header_t *h = (cell_exec_header_t *)dst;
	h->magic = CELL_EXEC_MAGIC; h->version = CELL_EXEC_VERSION; h->header_bytes = CELL_EXEC_HEADER_BYTES;
	h->instruction_bytes = CELL_EXEC_INSN_BYTES; h->code_bytes = (uint32_t)code_bytes; h->data_bytes = c->data_bytes;
	h->entry_pc = 0; h->capability_mask = c->capability_mask; h->memory_bytes = CELL_CC_TASK_MEMORY;
	uint64_t gas = (uint64_t)c->code_count * 128u + 1024u; if (gas > CELL_EXEC_GAS_MAX) gas = CELL_EXEC_GAS_MAX;
	h->gas_limit = (uint32_t)gas; h->flags = CELL_EXEC_F_NONE; h->total_bytes = (uint32_t)total;
	for (uint32_t i = 0; i < c->code_count; ++i) ((cell_exec_insn_t *)(dst + CELL_EXEC_HEADER_BYTES))[i] = c->code[i];
	uint8_t *data_dst = dst + CELL_EXEC_HEADER_BYTES + code_bytes;
	for (uint32_t i = 0; i < c->data_bytes; ++i) data_dst[i] = c->data[i];
	h->payload_crc32 = cell_exec_crc32(dst + CELL_EXEC_HEADER_BYTES, (size_t)payload);
	*output_bytes = (size_t)total; return 1;
}
