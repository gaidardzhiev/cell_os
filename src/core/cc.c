/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/cc.h"
#include "core/capability.h"
#include "core/cellexec.h"
#include "core/syscall.h"

#define CC_IDENT_MAX 63u
#define CC_STRING_MAX 512u
#define CC_TEMP_FIRST 8u
#define CC_TEMP_LAST 15u
#define CC_LOCALS 64u
#define CC_GLOBALS 32u
#define CC_FUNCS 64u
#define CC_PARAMS 8u
#define CC_STRUCTS 16u
#define CC_FIELDS 16u
#define CC_CALL_PATCHES 128u
#define CC_ADDR_PATCHES 128u
#define CC_INIT_MAX 256u
#define CC_PP_MACROS 32u
#define CC_PP_VALUE_MAX 127u
#define CC_PP_DEPTH 8u
#define CC_CALL_SCRATCH_DEPTH 4u
#define CC_CALL_SLOT_BYTES (CC_PARAMS * 8u)
#define CC_CALL_SCRATCH_BYTES (CC_CALL_SCRATCH_DEPTH * CC_CALL_SLOT_BYTES)
#define CC_GLOBAL_BASE (CELL_CC_TASK_MEMORY - CELL_CC_STATIC_MEMORY)
#define CC_ARRAY_UNSIZED 0xffffffffu

typedef enum {
	TOK_EOF = 0,
	TOK_IDENT = 256,
	TOK_NUMBER,
	TOK_STRING,
	TOK_EQ,
	TOK_NE,
	TOK_LE,
	TOK_GE,
	TOK_ANDAND,
	TOK_OROR,
	TOK_INC,
	TOK_DEC,
	TOK_ADD_EQ,
	TOK_SUB_EQ,
	TOK_MUL_EQ,
	TOK_DIV_EQ,
	TOK_MOD_EQ,
	TOK_SHL,
	TOK_SHR,
	TOK_ARROW
} token_kind_t;

typedef enum {
	BT_VOID = 0,
	BT_INT,
	BT_CHAR,
	BT_STRUCT
} base_type_t;

typedef struct {
	uint8_t base;
	uint8_t pointers;
	uint8_t struct_index;
} ctype_t;

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
	cell_cc_diag_t *diag;
} lexer_t;

typedef struct {
	char name[CC_IDENT_MAX + 1u];
	char value[CC_PP_VALUE_MAX + 1u];
} macro_t;

typedef struct {
	char name[CC_IDENT_MAX + 1u];
	ctype_t type;
	uint32_t offset;
	uint32_t array_len;
	uint16_t scope;
} local_t;

typedef struct {
	char name[CC_IDENT_MAX + 1u];
	ctype_t type;
	uint32_t offset;
	uint32_t array_len;
	uint8_t defined;
} global_t;

typedef struct {
	char name[CC_IDENT_MAX + 1u];
	ctype_t type;
	uint32_t offset;
	uint32_t array_len;
} field_t;

typedef struct {
	char name[CC_IDENT_MAX + 1u];
	field_t fields[CC_FIELDS];
	uint8_t field_count;
	uint32_t size;
	uint32_t align;
} struct_type_t;

typedef struct {
	char name[CC_IDENT_MAX + 1u];
	ctype_t return_type;
	ctype_t params[CC_PARAMS];
	uint8_t param_count;
	uint8_t defined;
	uint8_t declared;
	uint32_t entry_pc;
} function_t;

typedef struct {
	uint32_t pc;
	uint8_t func_index;
} call_patch_t;

typedef struct {
	uint32_t pc;
	uint8_t global_index;
} addr_patch_t;

typedef struct {
	uint32_t offset;
	uint64_t value;
	uint8_t size;
} init_op_t;

typedef struct {
	uint8_t reg;
	ctype_t type;
	uint32_t array_len;
	uint8_t lvalue;
	uint8_t const_zero;
} expr_t;

typedef struct {
	uint32_t continue_pc;
	uint32_t break_patches[32];
	uint8_t break_count;
} loop_t;

typedef struct {
	lexer_t lex;
	token_t tok;
	char preprocessed[CELL_CC_SOURCE_MAX + 1u];
	cell_exec_insn_t code[CELL_CC_MAX_CODE];
	uint32_t code_count;
	uint8_t data[CELL_CC_MAX_DATA];
	uint32_t data_bytes;
	local_t locals[CC_LOCALS];
	uint32_t local_count;
	uint32_t local_next;
	uint16_t scope_depth;
	global_t globals[CC_GLOBALS];
	uint32_t global_count;
	uint32_t global_next;
	struct_type_t structs[CC_STRUCTS];
	uint32_t struct_count;
	function_t funcs[CC_FUNCS];
	uint32_t func_count;
	call_patch_t call_patches[CC_CALL_PATCHES];
	uint32_t call_patch_count;
	addr_patch_t addr_patches[CC_ADDR_PATCHES];
	uint32_t addr_patch_count;
	init_op_t inits[CC_INIT_MAX];
	uint32_t init_count;
	macro_t macros[CC_PP_MACROS];
	uint32_t macro_count;
	uint8_t temp_mask;
	uint8_t call_scratch_depth;
	uint8_t current_func;
	uint8_t in_function;
	uint32_t frame_pc;
	uint64_t capability_mask;
	loop_t loops[8];
	uint8_t loop_depth;
	uint8_t main_seen;
	uint32_t entry_pc;
	cell_cc_diag_t *diag;
	int failed;
} compiler_t;

static compiler_t g_cc;

static ctype_t type_make(uint8_t base, uint8_t pointers, uint8_t si) {
	ctype_t t;
	t.base = base;
	t.pointers = pointers;
	t.struct_index = si;
	return t;
}
static ctype_t type_int(void) { return type_make(BT_INT, 0u, 0u);
}
static ctype_t type_char(void) { return type_make(BT_CHAR, 0u, 0u);
}
static ctype_t type_void(void) { return type_make(BT_VOID, 0u, 0u);
}
static int s_eq(const char *a, const char *b) {
	if (!a || !b) return 0;
	while (*a && *b) if (*a++ != *b++) return 0;
	return *a == 0 && *b == 0;
}
static size_t s_len(const char *s) { size_t n = 0;
while (s && s[n]) ++n;
return n;
}
static void s_copy(char *dst, size_t cap, const char *src) {
	if (!dst || !cap) return;
	size_t n = 0;
	while (src && src[n] && n + 1u < cap) { dst[n] = src[n];
	++n;
	}
	dst[n] = 0;
}
static void zero_bytes(void *p, size_t n) { uint8_t *b = (uint8_t *)p;
while (n--) *b++ = 0;
}
static uint32_t align_up(uint32_t n, uint32_t a) { return a <= 1u ? n : (n + a - 1u) & ~(a - 1u);
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
static int pp_fail(compiler_t *c, uint32_t line, const char *msg) {
	diag_set(c->diag, CELL_CC_LEX_ERROR, line, 1u, msg);
	c->failed = 1;
	return 0;
}
static int is_alpha(char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}
static int is_digit(char ch) { return ch >= '0' && ch <= '9';
}
static int is_alnum(char ch) { return is_alpha(ch) || is_digit(ch);
}
static int pp_macro_index(const compiler_t *c, const char *name) {
	for (uint32_t i = 0; i < c->macro_count; ++i) if (s_eq(c->macros[i].name, name)) return (int)i;
	return -1;
}
static int pp_append(compiler_t *c, size_t *out, char ch, uint32_t line) {
	if (*out >= CELL_CC_SOURCE_MAX) return pp_fail(c, line, "preprocessed source exceeds compiler limit");
	c->preprocessed[(*out)++] = ch;
	return 1;
}
static int pp_expand_fragment(compiler_t *c, const char *s, size_t n, size_t *out,
	uint32_t line, unsigned depth) {
	if (depth > 8u) return pp_fail(c, line, "macro expansion depth exceeded");
	size_t i = 0;
	while (i < n) {
		if (is_alpha(s[i])) {
			char name[CC_IDENT_MAX + 1u];
			size_t j = i, k = 0;
			while (j < n && is_alnum(s[j])) {
				if (k + 1u >= sizeof(name)) return pp_fail(c, line, "identifier too long in macro expansion");
				name[k++] = s[j++];
			}
			name[k] = 0;
			int mi = pp_macro_index(c, name);
			if (mi >= 0) {
				const char *v = c->macros[mi].value;
				if (!pp_expand_fragment(c, v, s_len(v), out, line, depth + 1u)) return 0;
			} else {
				for (size_t q = i; q < j; ++q) if (!pp_append(c, out, s[q], line)) return 0;
			}
			i = j;
			continue;
		}
		if (!pp_append(c, out, s[i++], line)) return 0;
	}
	return 1;
}
static int pp_expand_line(compiler_t *c, const char *s, size_t n, size_t *out,
	uint32_t line, int *block_comment) {
	size_t i = 0;
	while (i < n) {
		if (*block_comment) {
			if (i + 1u < n && s[i] == '*' && s[i + 1u] == '/') {
				if (!pp_append(c, out, '*', line) || !pp_append(c, out, '/', line)) return 0;
				i += 2u;
				*block_comment = 0;
				continue;
			}
			if (!pp_append(c, out, s[i++], line)) return 0;
			continue;
		}
		if (i + 1u < n && s[i] == '/' && s[i + 1u] == '*') {
			if (!pp_append(c, out, '/', line) || !pp_append(c, out, '*', line)) return 0;
			i += 2u;
			*block_comment = 1;
			continue;
		}
		if (i + 1u < n && s[i] == '/' && s[i + 1u] == '/') {
			while (i < n) if (!pp_append(c, out, s[i++], line)) return 0;
			break;
		}
		if (s[i] == '"' || s[i] == '\'') {
			char quote = s[i];
			if (!pp_append(c, out, s[i++], line)) return 0;
			while (i < n) {
				char ch = s[i++];
				if (!pp_append(c, out, ch, line)) return 0;
				if (ch == '\\' && i < n) { if (!pp_append(c, out, s[i++], line)) return 0;
				continue;
				}
				if (ch == quote) break;
			}
			continue;
		}
		if (is_alpha(s[i])) {
			size_t j = i + 1u;
			while (j < n && is_alnum(s[j])) ++j;
			if (!pp_expand_fragment(c, s + i, j - i, out, line, 0u)) return 0;
			i = j;
			continue;
		}
		if (!pp_append(c, out, s[i++], line)) return 0;
	}
	return 1;
}
static int pp_parse_int_value(const compiler_t *c, const char *s, int *value) {
	while (*s == ' ' || *s == '\t') ++s;
	if (s[0] == 'd' && s[1] == 'e' && s[2] == 'f' && s[3] == 'i' && s[4] == 'n' && s[5] == 'e' && s[6] == 'd') {
		s += 7;
		while (*s == ' ' || *s == '\t') ++s;
		if (*s == '(') ++s;
		char name[CC_IDENT_MAX + 1u];
		size_t n = 0;
		while (is_alnum(*s)) { if (n + 1u >= sizeof(name)) return 0;
		name[n++] = *s++;
		} name[n] = 0;
		*value = pp_macro_index(c, name) >= 0;
		return 1;
	}
	if (is_alpha(*s)) {
		char name[CC_IDENT_MAX + 1u];
		size_t n = 0;
		while (is_alnum(*s)) { if (n + 1u >= sizeof(name)) return 0;
		name[n++] = *s++;
		} name[n] = 0;
		int mi = pp_macro_index(c, name);
		if (mi < 0) { *value = 0;
		return 1;
		}
		s = c->macros[mi].value;
		while (*s == ' ' || *s == '\t') ++s;
	}
	int neg = 0;
	if (*s == '-') { neg = 1;
	++s;
	}
	if (!is_digit(*s)) return 0;
	long v = 0;
	while (is_digit(*s)) { v = v * 10 + (*s++ - '0');
	if (v > 2147483647L) return 0;
	}
	*value = neg ? -(int)v : (int)v;
	return 1;
}
static int preprocess(compiler_t *c, const char *src, size_t bytes) {
	c->macro_count = 0u;
	typedef struct { uint8_t parent, cond, seen_else;
	} cond_t;
	cond_t stack[CC_PP_DEPTH];
	uint32_t depth = 0;
	int active = 1, block_comment = 0;
	size_t pos = 0, out = 0;
	uint32_t line = 1u;
	while (pos < bytes) {
		size_t start = pos;
		while (pos < bytes && src[pos] != '\n') ++pos;
		size_t end = pos;
		if (pos < bytes && src[pos] == '\n') ++pos;
		size_t p = start;
		while (p < end && (src[p] == ' ' || src[p] == '\t' || src[p] == '\r')) ++p;
		int directive = !block_comment && p < end && src[p] == '#';
		if (directive) {
			++p;
			while (p < end && (src[p] == ' ' || src[p] == '\t')) ++p;
			char word[16];
			size_t wn = 0;
			while (p < end && is_alpha(src[p])) { if (wn + 1u < sizeof(word)) word[wn++] = src[p];
			++p;
			} word[wn] = 0;
			while (p < end && (src[p] == ' ' || src[p] == '\t')) ++p;
			if (s_eq(word, "if") || s_eq(word, "ifdef") || s_eq(word, "ifndef")) {
				if (depth >= CC_PP_DEPTH) return pp_fail(c, line, "conditional preprocessing nesting exceeded");
				int cond = 0;
				if (s_eq(word, "ifdef") || s_eq(word, "ifndef")) {
					char name[CC_IDENT_MAX + 1u];
					size_t n = 0;
					while (p < end && is_alnum(src[p])) { if (n + 1u >= sizeof(name)) return pp_fail(c, line, "preprocessor identifier too long");
					name[n++] = src[p++];
					} name[n] = 0;
					cond = pp_macro_index(c, name) >= 0;
					if (s_eq(word, "ifndef")) cond = !cond;
				} else {
					char expr[CC_PP_VALUE_MAX + 1u];
					size_t n = 0;
					while (p < end && n + 1u < sizeof(expr)) expr[n++] = src[p++];
					expr[n] = 0;
					if (!pp_parse_int_value(c, expr, &cond)) return pp_fail(c, line, "#if supports integer constants, macros, and defined(NAME)");
				}
				stack[depth].parent = (uint8_t)active;
				stack[depth].cond = (uint8_t)(cond != 0);
				stack[depth].seen_else = 0u;
				++depth;
				active = active && cond;
			} else if (s_eq(word, "else")) {
				if (!depth || stack[depth - 1u].seen_else) return pp_fail(c, line, "unexpected #else");
				stack[depth - 1u].seen_else = 1u;
				stack[depth - 1u].cond = (uint8_t)!stack[depth - 1u].cond;
				active = stack[depth - 1u].parent && stack[depth - 1u].cond;
			} else if (s_eq(word, "endif")) {
				if (!depth) return pp_fail(c, line, "unexpected #endif");
				--depth;
				active = stack[depth].parent;
			} else if (active && s_eq(word, "define")) {
				char name[CC_IDENT_MAX + 1u];
				size_t n = 0;
				while (p < end && is_alnum(src[p])) { if (n + 1u >= sizeof(name)) return pp_fail(c, line, "macro name too long");
				name[n++] = src[p++];
				} name[n] = 0;
				if (!n || (p < end && src[p] == '(')) return pp_fail(c, line, "only object-like #define is supported");
				while (p < end && (src[p] == ' ' || src[p] == '\t')) ++p;
				int mi = pp_macro_index(c, name);
				if (mi < 0) { if (c->macro_count >= CC_PP_MACROS) return pp_fail(c, line, "too many preprocessor macros");
				mi = (int)c->macro_count++;
				s_copy(c->macros[mi].name, sizeof(c->macros[mi].name), name);
				}
				size_t vn = end - p;
				while (vn && (src[p + vn - 1u] == ' ' || src[p + vn - 1u] == '\t' || src[p + vn - 1u] == '\r')) --vn;
				if (vn > CC_PP_VALUE_MAX) return pp_fail(c, line, "macro replacement too long");
				for (size_t i = 0; i < vn; ++i) c->macros[mi].value[i] = src[p + i];
				c->macros[mi].value[vn] = 0;
			} else if (active && s_eq(word, "undef")) {
				char name[CC_IDENT_MAX + 1u];
				size_t n = 0;
				while (p < end && is_alnum(src[p])) { if (n + 1u >= sizeof(name)) return pp_fail(c, line, "macro name too long");
				name[n++] = src[p++];
				} name[n] = 0;
				int mi = pp_macro_index(c, name);
				if (mi >= 0) { for (uint32_t i = (uint32_t)mi + 1u; i < c->macro_count; ++i) c->macros[i - 1u] = c->macros[i];
				--c->macro_count;
				}
			} else if (active && s_eq(word, "include")) {
				/* Headers expose builtins and constants directly in the bootstrap environment. */
			} else if (active && word[0]) return pp_fail(c, line, "unsupported preprocessor directive");
			if (!pp_append(c, &out, '\n', line)) return 0;
		} else if (active) {
			if (!pp_expand_line(c, src + start, end - start, &out, line, &block_comment) || !pp_append(c, &out, '\n', line)) return 0;
		} else if (!pp_append(c, &out, '\n', line)) return 0;
		++line;
	}
	if (depth) return pp_fail(c, line, "unterminated conditional preprocessing block");
	if (block_comment) return pp_fail(c, line, "unterminated comment");
	c->preprocessed[out] = 0;
	return 1;
}

static char lx_peek(const lexer_t *l) { return l->pos < l->bytes ? l->src[l->pos] : 0;
}
static char lx_peek2(const lexer_t *l) { return l->pos + 1u < l->bytes ? l->src[l->pos + 1u] : 0;
}
static char lx_get(lexer_t *l) {
	if (l->pos >= l->bytes) return 0;
	char ch = l->src[l->pos++];
	if (ch == '\n') { ++l->line;
	l->column = 1u;
	} else ++l->column;
	return ch;
}
static int hex_digit(char ch) {
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}
static int escaped_char(lexer_t *l, uint32_t line, uint32_t col, char *out) {
	char ch = lx_get(l);
	if (!ch) { diag_set(l->diag, CELL_CC_LEX_ERROR, line, col, "unterminated character escape");
	return 0;
	}
	switch (ch) {
	case 'n': *out = '\n';
	return 1;
	case 'r': *out = '\r';
	return 1;
	case 't': *out = '\t';
	return 1;
	case '0': *out = 0;
	return 1;
	case '\\': *out = '\\';
	return 1;
	case '\'': *out = '\'';
	return 1;
	case '"': *out = '"';
	return 1;
	default: diag_set(l->diag, CELL_CC_LEX_ERROR, line, col, "unsupported character escape");
	return 0;
	}
}
static int skip_space_comments(lexer_t *l) {
	for (;;) {
		char ch = lx_peek(l);
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') { (void)lx_get(l);
		continue;
		}
		if (ch == '/' && lx_peek2(l) == '/') { (void)lx_get(l);
		(void)lx_get(l);
		while (lx_peek(l) && lx_peek(l) != '\n') (void)lx_get(l);
		continue;
		}
		if (ch == '/' && lx_peek2(l) == '*') {
			uint32_t line = l->line, col = l->column;
			(void)lx_get(l);
			(void)lx_get(l);
			while (lx_peek(l) && !(lx_peek(l) == '*' && lx_peek2(l) == '/')) (void)lx_get(l);
			if (!lx_peek(l)) { diag_set(l->diag, CELL_CC_LEX_ERROR, line, col, "unterminated comment");
			return 0;
			}
			(void)lx_get(l);
			(void)lx_get(l);
			continue;
		}
		return 1;
	}
}
static int lex_next(lexer_t *l, token_t *t) {
	zero_bytes(t, sizeof(*t));
	if (!skip_space_comments(l)) return 0;
	t->line = l->line;
	t->column = l->column;
	char ch = lx_peek(l);
	if (!ch) { t->kind = TOK_EOF;
	return 1;
	}
	if (is_alpha(ch)) {
		t->kind = TOK_IDENT;
		while (is_alnum(lx_peek(l))) { if (t->text_len + 1u >= CC_IDENT_MAX + 1u) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "identifier too long");
		return 0;
		} t->text[t->text_len++] = lx_get(l);
		}
		t->text[t->text_len] = 0;
		return 1;
	}
	if (is_digit(ch)) {
		int64_t value = 0;
		int base = 10;
		if (ch == '0' && (lx_peek2(l) == 'x' || lx_peek2(l) == 'X')) { (void)lx_get(l);
		(void)lx_get(l);
		base = 16;
		if (hex_digit(lx_peek(l)) < 0) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "invalid hexadecimal constant");
		return 0;
		} while (hex_digit(lx_peek(l)) >= 0) value = value * 16 + hex_digit(lx_get(l));
		}
		else while (is_digit(lx_peek(l))) value = value * base + (lx_get(l) - '0');
		if (value > 0x7fffffffll) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "integer constant out of range");
		return 0;
		}
		t->kind = TOK_NUMBER;
		t->number = (int32_t)value;
		return 1;
	}
	if (ch == '\'') {
		(void)lx_get(l);
		char v = lx_get(l);
		if (!v || v == '\n' || v == '\r') { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "invalid character constant");
		return 0;
		}
		if (v == '\\' && !escaped_char(l, t->line, t->column, &v)) return 0;
		if (lx_get(l) != '\'') { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "character constant must contain one byte");
		return 0;
		}
		t->kind = TOK_NUMBER;
		t->number = (unsigned char)v;
		return 1;
	}
	if (ch == '"') {
		t->kind = TOK_STRING;
		(void)lx_get(l);
		while ((ch = lx_get(l)) != 0 && ch != '"') {
			if (ch == '\n' || ch == '\r') { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "newline in string literal");
			return 0;
			}
			if (ch == '\\' && !escaped_char(l, t->line, t->column, &ch)) return 0;
			if (t->text_len + 1u >= sizeof(t->text)) { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "string literal too long");
			return 0;
			}
			t->text[t->text_len++] = ch;
		}
		if (ch != '"') { diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "unterminated string literal");
		return 0;
		}
		t->text[t->text_len] = 0;
		return 1;
	}
#define TWO(a,b,k) if (ch == (a) && lx_peek2(l) == (b)) { (void)lx_get(l); (void)lx_get(l); t->kind = (k); return 1; }
	TWO('=', '=', TOK_EQ) TWO('!', '=', TOK_NE) TWO('<', '=', TOK_LE) TWO('>', '=', TOK_GE)
	TWO('&', '&', TOK_ANDAND) TWO('|', '|', TOK_OROR) TWO('+', '+', TOK_INC) TWO('-', '-', TOK_DEC)
	TWO('+', '=', TOK_ADD_EQ) TWO('-', '=', TOK_SUB_EQ) TWO('*', '=', TOK_MUL_EQ) TWO('/', '=', TOK_DIV_EQ) TWO('%', '=', TOK_MOD_EQ)
	TWO('<', '<', TOK_SHL) TWO('>', '>', TOK_SHR) TWO('-', '>', TOK_ARROW)
#undef TWO
	if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' || ch == '=' || ch == ';' || ch == ',' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == '<' || ch == '>' || ch == '!' || ch == '|' || ch == '&' || ch == '^' || ch == '~' || ch == '.') { t->kind = (unsigned char)lx_get(l);
	return 1;
	}
	diag_set(l->diag, CELL_CC_LEX_ERROR, t->line, t->column, "unsupported character");
	return 0;
}

static int next(compiler_t *c) { if (!lex_next(&c->lex, &c->tok)) { c->failed = 1;
return 0;
} return 1;
}
static int fail_at(compiler_t *c, cell_cc_status_t status, const char *msg) { diag_set(c->diag, status, c->tok.line, c->tok.column, msg);
c->failed = 1;
return 0;
}
static int tok_ident(const compiler_t *c, const char *s) { return c->tok.kind == TOK_IDENT && s_eq(c->tok.text, s);
}
static int expect(compiler_t *c, int kind, const char *msg) { if (c->tok.kind != kind) return fail_at(c, CELL_CC_PARSE_ERROR, msg);
return next(c);
}
static int emit(compiler_t *c, uint8_t opcode, uint8_t dst, uint8_t a, uint8_t b, int32_t imm) {
	if (c->code_count >= CELL_CC_MAX_CODE) return fail_at(c, CELL_CC_TOO_COMPLEX, "program has too many instructions");
	cell_exec_insn_t *in = &c->code[c->code_count++];
	in->opcode = opcode;
	in->dst = dst;
	in->a = a;
	in->b = b;
	in->imm = imm;
	return 1;
}
static int patch_branch(compiler_t *c, uint32_t pc, uint32_t target) {
	if (pc >= c->code_count) return 0;
	int64_t rel = (int64_t)target - ((int64_t)pc + 1);
	if (rel < -0x80000000ll || rel > 0x7fffffffll) return 0;
	c->code[pc].imm = (int32_t)rel;
	return 1;
}
static int emit_jump_to(compiler_t *c, uint32_t target) {
	uint32_t pc = c->code_count;
	if (!emit(c, CELL_EXEC_OP_JMP, 0, 0, 0, 0)) return 0;
	return patch_branch(c, pc, target);
}
static int alloc_temp(compiler_t *c, uint8_t *r) {
	for (uint8_t i = CC_TEMP_FIRST; i <= CC_TEMP_LAST; ++i) { uint8_t bit = (uint8_t)(1u << (i - CC_TEMP_FIRST));
	if (!(c->temp_mask & bit)) { c->temp_mask |= bit;
	*r = i;
	return 1;
	} }
	return fail_at(c, CELL_CC_TOO_COMPLEX, "expression exceeds temporary register budget");
}
static void free_temp(compiler_t *c, uint8_t r) { if (r >= CC_TEMP_FIRST && r <= CC_TEMP_LAST) c->temp_mask &= (uint8_t)~(1u << (r - CC_TEMP_FIRST));
}
static int type_equal(ctype_t a, ctype_t b) { return a.base == b.base && a.pointers == b.pointers && (a.base != BT_STRUCT || a.struct_index == b.struct_index);
}
static int type_is_integer(ctype_t t) { return t.pointers == 0u && (t.base == BT_INT || t.base == BT_CHAR);
}
static int type_is_pointer(ctype_t t) { return t.pointers != 0u;
}
static int type_is_struct(ctype_t t) { return t.pointers == 0u && t.base == BT_STRUCT;
}
static int type_compatible(ctype_t dst, ctype_t src, int src_zero) {
	if (type_equal(dst, src)) return 1;
	if (type_is_integer(dst) && type_is_integer(src)) return 1;
	if (type_is_pointer(dst) && type_is_pointer(src)) {
		if (dst.pointers == src.pointers && (dst.base == BT_VOID || src.base == BT_VOID)) return 1;
		return type_equal(dst, src);
	}
	if (type_is_pointer(dst) && type_is_integer(src) && src_zero) return 1;
	return 0;
}
static uint32_t type_align(const compiler_t *c, ctype_t t) {
	if (t.pointers) return 8u;
	if (t.base == BT_CHAR) return 1u;
	if (t.base == BT_INT) return 4u;
	if (t.base == BT_STRUCT && t.struct_index < c->struct_count) return c->structs[t.struct_index].align;
	return 1u;
}
static uint32_t type_size(const compiler_t *c, ctype_t t) {
	if (t.pointers) return 8u;
	if (t.base == BT_CHAR) return 1u;
	if (t.base == BT_INT) return 4u;
	if (t.base == BT_STRUCT && t.struct_index < c->struct_count) return c->structs[t.struct_index].size;
	return 0u;
}
static uint32_t object_size(const compiler_t *c, ctype_t t, uint32_t array_len) {
	uint32_t n = type_size(c, t);
	if (!n) return 0u;
	if (array_len) { if (array_len == CC_ARRAY_UNSIZED || array_len > 0xffffffffu / n) return 0u;
	n *= array_len;
	} return n;
}
static int find_struct(const compiler_t *c, const char *name, uint8_t *index) {
	for (uint32_t i = 0; i < c->struct_count; ++i) if (s_eq(c->structs[i].name, name)) { if (index) *index = (uint8_t)i;
	return 1;
	} return 0;
}
static int find_field(const compiler_t *c, uint8_t si, const char *name, field_t *out) {
	if (si >= c->struct_count) return 0;
	const struct_type_t *s = &c->structs[si];
	for (uint32_t i = 0; i < s->field_count; ++i) if (s_eq(s->fields[i].name, name)) { if (out) *out = s->fields[i];
	return 1;
	} return 0;
}
static int find_local(const compiler_t *c, const char *name, uint32_t *index) {
	for (uint32_t i = c->local_count; i > 0; --i) if (s_eq(c->locals[i - 1u].name, name)) { if (index) *index = i - 1u;
	return 1;
	} return 0;
}
static int find_global(const compiler_t *c, const char *name, uint32_t *index) {
	for (uint32_t i = 0; i < c->global_count; ++i) if (s_eq(c->globals[i].name, name)) { if (index) *index = i;
	return 1;
	} return 0;
}
static int find_func(const compiler_t *c, const char *name, uint8_t *index) {
	for (uint32_t i = 0; i < c->func_count; ++i) if (s_eq(c->funcs[i].name, name)) { if (index) *index = (uint8_t)i;
	return 1;
	} return 0;
}
static int add_local(compiler_t *c, const char *name, ctype_t type, uint32_t array_len, uint32_t *index) {
	for (uint32_t i = c->local_count; i > 0; --i) {
		if (c->locals[i - 1u].scope < c->scope_depth) break;
		if (s_eq(c->locals[i - 1u].name, name)) return fail_at(c, CELL_CC_PARSE_ERROR, "duplicate local variable");
	}
	if (c->local_count >= CC_LOCALS) return fail_at(c, CELL_CC_TOO_MANY_VARIABLES, "too many local variables and parameters");
	uint32_t size = object_size(c, type, array_len);
	if (!size) return fail_at(c, CELL_CC_PARSE_ERROR, "invalid or incomplete object type");
	uint32_t align = type_align(c, type);
	c->local_next = align_up(c->local_next, align);
	if (c->local_next > CC_GLOBAL_BASE || size > CC_GLOBAL_BASE - c->local_next) return fail_at(c, CELL_CC_TOO_COMPLEX, "function frame exceeds task memory budget");
	local_t *v = &c->locals[c->local_count];
	zero_bytes(v, sizeof(*v));
	s_copy(v->name, sizeof(v->name), name);
	v->type = type;
	v->offset = c->local_next;
	v->array_len = array_len;
	v->scope = c->scope_depth;
	c->local_next += size;
	if (index) *index = c->local_count;
	++c->local_count;
	return 1;
}
static int add_global_placeholder(compiler_t *c, const char *name, ctype_t type, uint32_t array_len, uint32_t *index) {
	uint32_t gi;
	if (find_global(c, name, &gi)) { if (!type_equal(c->globals[gi].type, type) || c->globals[gi].array_len != array_len) return fail_at(c, CELL_CC_PARSE_ERROR, "conflicting global declaration");
	if (index) *index = gi;
	return 1;
	}
	if (c->global_count >= CC_GLOBALS) return fail_at(c, CELL_CC_TOO_MANY_VARIABLES, "too many global objects");
	global_t *g = &c->globals[c->global_count];
	zero_bytes(g, sizeof(*g));
	s_copy(g->name, sizeof(g->name), name);
	g->type = type;
	g->array_len = array_len;
	if (index) *index = c->global_count;
	++c->global_count;
	return 1;
}
static int patch_global_addresses(compiler_t *c, uint32_t gi) {
	global_t *g = &c->globals[gi];
	if (!g->defined) return 1;
	for (uint32_t i = 0; i < c->addr_patch_count; ++i) if (c->addr_patches[i].global_index == gi) c->code[c->addr_patches[i].pc].imm = (int32_t)(CC_GLOBAL_BASE + g->offset);
	return 1;
}
static int define_global(compiler_t *c, uint32_t gi, uint32_t array_len) {
	global_t *g = &c->globals[gi];
	if (g->defined) return fail_at(c, CELL_CC_PARSE_ERROR, "duplicate global definition");
	g->array_len = array_len;
	uint32_t size = object_size(c, g->type, array_len);
	if (!size) return fail_at(c, CELL_CC_PARSE_ERROR, "invalid global object type");
	c->global_next = align_up(c->global_next, type_align(c, g->type));
	if (c->global_next > CELL_CC_STATIC_MEMORY || size > CELL_CC_STATIC_MEMORY - c->global_next) return fail_at(c, CELL_CC_TOO_COMPLEX, "global objects exceed static memory region");
	g->offset = c->global_next;
	c->global_next += size;
	g->defined = 1u;
	return patch_global_addresses(c, gi);
}
static int add_function(compiler_t *c, const char *name, ctype_t ret, const ctype_t *params, uint8_t argc, uint8_t declared, uint8_t *index) {
	uint8_t fi;
	if (find_func(c, name, &fi)) {
		function_t *f = &c->funcs[fi];
		if (f->param_count != argc || !type_equal(f->return_type, ret)) return fail_at(c, CELL_CC_PARSE_ERROR, "conflicting function declaration");
		for (uint8_t i = 0; i < argc; ++i) if (!type_compatible(f->params[i], params[i], 0) || !type_compatible(params[i], f->params[i], 0)) return fail_at(c, CELL_CC_PARSE_ERROR, "conflicting function parameter type");
		if (declared) f->declared = 1u;
		if (index) *index = fi;
		return 1;
	}
	if (c->func_count >= CC_FUNCS) return fail_at(c, CELL_CC_TOO_COMPLEX, "too many user-defined functions");
	function_t *f = &c->funcs[c->func_count];
	zero_bytes(f, sizeof(*f));
	s_copy(f->name, sizeof(f->name), name);
	f->return_type = ret;
	f->param_count = argc;
	f->declared = declared;
	for (uint8_t i = 0; i < argc; ++i) f->params[i] = params[i];
	if (index) *index = (uint8_t)c->func_count;
	++c->func_count;
	return 1;
}
static int patch_function_calls(compiler_t *c, uint8_t fi) {
	function_t *f = &c->funcs[fi];
	for (uint32_t i = 0; i < c->call_patch_count; ++i) if (c->call_patches[i].func_index == fi) { uint32_t pc = c->call_patches[i].pc;
	uint8_t argc = CELL_EXEC_CALL_ARGC(c->code[pc].imm);
	c->code[pc].imm = CELL_EXEC_CALL_PACK(f->entry_pc, argc);
	} return 1;
}
static int add_data(compiler_t *c, const char *s, uint32_t n, int suffix, uint32_t *offset) {
	uint32_t extra = suffix >= 0 ? 1u : 0u;
	if (c->data_bytes + n + extra > CELL_CC_MAX_DATA) return fail_at(c, CELL_CC_OUTPUT_TOO_LARGE, "string data exceeds compiler limit");
	if (offset) *offset = c->data_bytes;
	for (uint32_t i = 0; i < n; ++i) c->data[c->data_bytes++] = (uint8_t)s[i];
	if (suffix >= 0) c->data[c->data_bytes++] = (uint8_t)suffix;
	return 1;
}
static int add_init(compiler_t *c, uint32_t offset, uint8_t size, uint64_t value) {
	if (c->init_count >= CC_INIT_MAX) return fail_at(c, CELL_CC_TOO_COMPLEX, "too many global initializer elements");
	init_op_t *op = &c->inits[c->init_count++];
	op->offset = offset;
	op->size = size;
	op->value = value;
	return 1;
}
static int emit_symbol_address(compiler_t *c, const char *name, uint8_t *reg, ctype_t *type, uint32_t *array_len) {
	uint32_t li;
	if (find_local(c, name, &li)) {
		if (!alloc_temp(c, reg) || !emit(c, CELL_EXEC_OP_ADDRL, *reg, 0, 0, (int32_t)c->locals[li].offset)) return 0;
		if (type) *type = c->locals[li].type;
		if (array_len) *array_len = c->locals[li].array_len;
		return 1;
	}
	uint32_t gi;
	if (find_global(c, name, &gi)) {
		if (!alloc_temp(c, reg)) return 0;
		uint32_t pc = c->code_count;
		int32_t addr = c->globals[gi].defined ? (int32_t)(CC_GLOBAL_BASE + c->globals[gi].offset) : 0;
		if (!emit(c, CELL_EXEC_OP_MOVI, *reg, 0, 0, addr)) return 0;
		if (!c->globals[gi].defined) { if (c->addr_patch_count >= CC_ADDR_PATCHES) return fail_at(c, CELL_CC_TOO_COMPLEX, "too many unresolved global references");
		c->addr_patches[c->addr_patch_count].pc = pc;
		c->addr_patches[c->addr_patch_count].global_index = (uint8_t)gi;
		++c->addr_patch_count;
		}
		if (type) *type = c->globals[gi].type;
		if (array_len) *array_len = c->globals[gi].array_len;
		return 1;
	}
	return 0;
}
static int parse_decl_spec(compiler_t *c, ctype_t *base) {
	if (tok_ident(c, "int")) { *base = type_int();
	return next(c);
	}
	if (tok_ident(c, "char")) { *base = type_char();
	return next(c);
	}
	if (tok_ident(c, "void")) { *base = type_void();
	return next(c);
	}
	if (tok_ident(c, "struct")) {
		if (!next(c) || c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected struct tag");
		uint8_t si;
		if (!find_struct(c, c->tok.text, &si)) return fail_at(c, CELL_CC_PARSE_ERROR, "unknown struct type");
		*base = type_make(BT_STRUCT, 0u, si);
		return next(c);
	}
	return fail_at(c, CELL_CC_PARSE_ERROR, "expected C type");
}
static int parse_declarator_type(compiler_t *c, ctype_t base, ctype_t *type) {
	*type = base;
	while (c->tok.kind == '*') { if (type->pointers >= 3u) return fail_at(c, CELL_CC_PARSE_ERROR, "pointer depth exceeds compiler limit");
	++type->pointers;
	if (!next(c)) return 0;
	} return 1;
}
static int parse_array_suffix(compiler_t *c, uint32_t *array_len) {
	*array_len = 0u;
	if (c->tok.kind != '[') return 1;
	if (!next(c)) return 0;
	if (c->tok.kind == ']') { *array_len = CC_ARRAY_UNSIZED;
	return next(c);
	}
	if (c->tok.kind != TOK_NUMBER || c->tok.number <= 0) return fail_at(c, CELL_CC_PARSE_ERROR, "array bound must be a positive integer constant");
	*array_len = (uint32_t)c->tok.number;
	if (!next(c)) return 0;
	return expect(c, ']', "expected ']' after array bound");
}
static int emit_load(compiler_t *c, uint8_t dst, uint8_t addr, ctype_t t) {
	uint32_t size = type_size(c, t);
	uint8_t op = size == 1u ? CELL_EXEC_OP_LOAD8 : size == 4u ? CELL_EXEC_OP_LOAD32 : size == 8u ? CELL_EXEC_OP_LOAD64 : 0u;
	if (!op) return fail_at(c, CELL_CC_PARSE_ERROR, "aggregate value requires member or address access");
	return emit(c, op, dst, addr, 0, 0);
}
static int emit_store(compiler_t *c, uint8_t addr, uint8_t value, ctype_t t) {
	uint32_t size = type_size(c, t);
	uint8_t op = size == 1u ? CELL_EXEC_OP_STORE8 : size == 4u ? CELL_EXEC_OP_STORE32 : size == 8u ? CELL_EXEC_OP_STORE64 : 0u;
	if (!op) return fail_at(c, CELL_CC_PARSE_ERROR, "aggregate assignment is not supported");
	return emit(c, op, 0, addr, value, 0);
}
static int expr_rvalue(compiler_t *c, expr_t *e) {
	if (!e->lvalue) return 1;
	if (e->array_len) { e->type.pointers++;
	e->array_len = 0u;
	e->lvalue = 0u;
	e->const_zero = 0u;
	return 1;
	}
	if (type_is_struct(e->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "struct value requires member or address access");
	uint8_t dst;
	if (!alloc_temp(c, &dst) || !emit_load(c, dst, e->reg, e->type)) return 0;
	free_temp(c, e->reg);
	e->reg = dst;
	e->lvalue = 0u;
	e->const_zero = 0u;
	return 1;
}
static int expr_truth(compiler_t *c, expr_t *e) {
	if (!expr_rvalue(c, e)) return 0;
	if (!type_is_integer(e->type) && !type_is_pointer(e->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "condition requires scalar expression");
	return 1;
}
static int named_constant(const char *name, int32_t *value) {
	static const struct { const char *name;
	int32_t value;
	} table[] = {
		{"STDIN_FILENO", CELL_STDIN_FILENO}, {"STDOUT_FILENO", CELL_STDOUT_FILENO}, {"STDERR_FILENO", CELL_STDERR_FILENO},
		{"O_RDONLY", CELL_O_RDONLY}, {"O_WRONLY", CELL_O_WRONLY}, {"O_RDWR", CELL_O_RDWR}, {"O_CREAT", CELL_O_CREAT}, {"O_TRUNC", CELL_O_TRUNC}, {"O_APPEND", CELL_O_APPEND},
		{"SEEK_SET", CELL_SEEK_SET}, {"SEEK_CUR", CELL_SEEK_CUR}, {"SEEK_END", CELL_SEEK_END},
		{"EPERM", CELL_EPERM}, {"ENOENT", CELL_ENOENT}, {"EIO", CELL_EIO}, {"EBADF", CELL_EBADF}, {"ENOMEM", CELL_ENOMEM}, {"EACCES", CELL_EACCES}, {"EFAULT", CELL_EFAULT}, {"EEXIST", CELL_EEXIST}, {"ENOTDIR", CELL_ENOTDIR}, {"EISDIR", CELL_EISDIR}, {"EINVAL", CELL_EINVAL}, {"EMFILE", CELL_EMFILE}, {"EFBIG", CELL_EFBIG}, {"ENOSPC", CELL_ENOSPC}, {"ESPIPE", CELL_ESPIPE}, {"EROFS", CELL_EROFS}, {"ENOTEMPTY", CELL_ENOTEMPTY}
	};
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) if (s_eq(name, table[i].name)) { if (value) *value = table[i].value;
	return 1;
	} return 0;
}
static cell_capability_id_t cap_from_name(const char *name) {
	static const struct { const char *name;
	cell_capability_id_t id;
	} table[] = {
		{"system.status", CELL_CAP_SYSTEM_STATUS}, {"cpu.info", CELL_CAP_CPU_INFO}, {"memory.status", CELL_CAP_MEMORY_STATUS}, {"storage.list", CELL_CAP_STORAGE_LIST}, {"storage.list_dir", CELL_CAP_STORAGE_LIST_DIR}, {"storage.pwd", CELL_CAP_STORAGE_PWD}, {"network.list", CELL_CAP_NETWORK_LIST}, {"gpu.info", CELL_CAP_GPU_INFO}, {"usb.list", CELL_CAP_USB_LIST}, {"display.info", CELL_CAP_DISPLAY_INFO}, {"power.status", CELL_CAP_POWER_STATUS}
	};
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) if (s_eq(name, table[i].name)) return table[i].id;
	return CELL_CAP_INVALID;
}

static int parse_expr(compiler_t *c, expr_t *out);
static int parse_assignment(compiler_t *c, expr_t *out);

static int sys_builtin_nr_name(const char *name) {
	if (s_eq(name, "open")) return CELL_EXEC_SYS_OPEN;
	if (s_eq(name, "close")) return CELL_EXEC_SYS_CLOSE;
	if (s_eq(name, "read")) return CELL_EXEC_SYS_READ;
	if (s_eq(name, "write")) return CELL_EXEC_SYS_WRITE;
	if (s_eq(name, "lseek")) return CELL_EXEC_SYS_LSEEK;
	if (s_eq(name, "malloc")) return CELL_EXEC_SYS_MALLOC;
	if (s_eq(name, "free")) return CELL_EXEC_SYS_FREE;
	if (s_eq(name, "compile")) return CELL_EXEC_SYS_COMPILE;
	if (s_eq(name, "install_exec")) return CELL_EXEC_SYS_INSTALL_EXEC;
	return 0;
}
static int pointer_to_char(ctype_t t) { return t.pointers == 1u && (t.base == BT_CHAR || t.base == BT_VOID);
}
static int parse_sys_call(compiler_t *c, const char *name, expr_t *out) {
	int nr = sys_builtin_nr_name(name);
	if (!nr) return 0;
	if (!expect(c, '(', "expected '(' after system function")) return 0;
	expr_t args[3];
	zero_bytes(args, sizeof(args));
	unsigned argc = nr == CELL_EXEC_SYS_CLOSE || nr == CELL_EXEC_SYS_MALLOC || nr == CELL_EXEC_SYS_FREE ? 1u :
		(nr == CELL_EXEC_SYS_OPEN || nr == CELL_EXEC_SYS_COMPILE || nr == CELL_EXEC_SYS_INSTALL_EXEC ? 2u : 3u);
	for (unsigned i = 0; i < argc; ++i) {
		if (i && !expect(c, ',', "expected ',' in system function")) return 0;
		if (!parse_assignment(c, &args[i]) || !expr_rvalue(c, &args[i])) return 0;
	}
	if (!expect(c, ')', "expected ')' after system function")) return 0;
	if (nr == CELL_EXEC_SYS_OPEN && (!pointer_to_char(args[0].type) || !type_is_integer(args[1].type))) return fail_at(c, CELL_CC_PARSE_ERROR, "open requires char pointer and integer flags");
	if (nr == CELL_EXEC_SYS_COMPILE && (!pointer_to_char(args[0].type) || !pointer_to_char(args[1].type))) return fail_at(c, CELL_CC_PARSE_ERROR, "compile requires source and output path pointers");
	if (nr == CELL_EXEC_SYS_INSTALL_EXEC && (!pointer_to_char(args[0].type) || !pointer_to_char(args[1].type))) return fail_at(c, CELL_CC_PARSE_ERROR, "install_exec requires input and output path pointers");
	if (nr == CELL_EXEC_SYS_CLOSE && !type_is_integer(args[0].type)) return fail_at(c, CELL_CC_PARSE_ERROR, "close requires integer file descriptor");
	if ((nr == CELL_EXEC_SYS_READ || nr == CELL_EXEC_SYS_WRITE) && (!type_is_integer(args[0].type) || !type_is_pointer(args[1].type) || !type_is_integer(args[2].type))) return fail_at(c, CELL_CC_PARSE_ERROR, "read/write require fd, pointer, and integer count");
	if (nr == CELL_EXEC_SYS_LSEEK && (!type_is_integer(args[0].type) || !type_is_integer(args[1].type) || !type_is_integer(args[2].type))) return fail_at(c, CELL_CC_PARSE_ERROR, "lseek requires integer fd, offset, and whence");
	if (nr == CELL_EXEC_SYS_MALLOC && !type_is_integer(args[0].type)) return fail_at(c, CELL_CC_PARSE_ERROR, "malloc requires integer byte count");
	if (nr == CELL_EXEC_SYS_FREE && !type_is_pointer(args[0].type) && !args[0].const_zero) return fail_at(c, CELL_CC_PARSE_ERROR, "free requires pointer");
	uint8_t dst = args[0].reg;
	if (!emit(c, CELL_EXEC_OP_SYSCALL, dst, argc >= 1u ? args[0].reg : 0u, argc >= 2u ? args[1].reg : 0u, argc >= 3u ? CELL_EXEC_SYSCALL_PACK(nr, args[2].reg) : CELL_EXEC_SYSCALL_PACK(nr, 0u))) return 0;
	for (unsigned i = 1u; i < argc; ++i) free_temp(c, args[i].reg);
	out->reg = dst;
	out->type = nr == CELL_EXEC_SYS_MALLOC ? type_make(BT_VOID, 1u, 0u) : type_int();
	out->array_len = 0u;
	out->lvalue = 0u;
	out->const_zero = 0u;
	return 1;
}
static int parse_library_call(compiler_t *c, const char *name, expr_t *out) {
	int which = s_eq(name, "strlen") ? 1 : s_eq(name, "atoi") ? 2 : s_eq(name, "strcmp") ? 3 : s_eq(name, "puts") ? 4 : s_eq(name, "putchar") ? 5 : s_eq(name, "cell_capability") ? 6 : 0;
	if (!which) return 0;
	if (!expect(c, '(', "expected '(' after library function")) return 0;
	if (which == 6) {
		if (c->tok.kind != TOK_STRING) return fail_at(c, CELL_CC_PARSE_ERROR, "cell_capability requires a string literal capability name");
		cell_capability_id_t id = cap_from_name(c->tok.text);
		if (id == CELL_CAP_INVALID) return fail_at(c, CELL_CC_UNKNOWN_CAPABILITY, "unknown capability name");
		if (!next(c) || !expect(c, ')', "expected ')' after cell_capability")) return 0;
		uint8_t r;
		if (!alloc_temp(c, &r) || !emit(c, CELL_EXEC_OP_CAP, r, 0, 0, (int32_t)id)) return 0;
		c->capability_mask |= 1ull << (uint32_t)id;
		out->reg = r;
		out->type = type_int();
		out->array_len = 0u;
		out->lvalue = 0u;
		out->const_zero = 0u;
		return 1;
	}
	expr_t a;
	if (!parse_assignment(c, &a) || !expr_rvalue(c, &a)) return 0;
	if (which == 3) {
		expr_t b;
		if (!expect(c, ',', "expected ',' in strcmp") || !parse_assignment(c, &b) || !expr_rvalue(c, &b)) return 0;
		if (!pointer_to_char(a.type) || !pointer_to_char(b.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "strcmp requires char pointers");
		if (!expect(c, ')', "expected ')' after strcmp") || !emit(c, CELL_EXEC_OP_STRCMP, a.reg, a.reg, b.reg, 0)) return 0;
		free_temp(c, b.reg);
		a.type = type_int();
		*out = a;
		return 1;
	}
	if (!expect(c, ')', "expected ')' after library function")) return 0;
	if (which == 1 || which == 2 || which == 4) {
		if (!pointer_to_char(a.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "string library function requires char pointer");
		if (which == 1) { if (!emit(c, CELL_EXEC_OP_STRLEN, a.reg, a.reg, 0, 0)) return 0;
		a.type = type_int();
		}
		else if (which == 2) { if (!emit(c, CELL_EXEC_OP_ATOI, a.reg, a.reg, 0, 0)) return 0;
		a.type = type_int();
		}
		else { if (!emit(c, CELL_EXEC_OP_PUTSM, 0, a.reg, 0, 0) || !emit(c, CELL_EXEC_OP_MOVI, a.reg, 0, 0, 0)) return 0;
		a.type = type_int();
		a.const_zero = 1u;
		}
	} else {
		if (!type_is_integer(a.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "putchar requires integer argument");
		if (!emit(c, CELL_EXEC_OP_PUTC, 0, a.reg, 0, 0)) return 0;
		a.type = type_int();
	}
	*out = a;
	return 1;
}
static int parse_user_call(compiler_t *c, const char *name, expr_t *out) {
	if (!c->in_function) return fail_at(c, CELL_CC_PARSE_ERROR, "function calls are only valid inside functions");
	if (c->call_scratch_depth >= CC_CALL_SCRATCH_DEPTH) return fail_at(c, CELL_CC_TOO_COMPLEX, "nested function-call expression depth exceeded");
	uint8_t slot = c->call_scratch_depth++;
	if (!expect(c, '(', "expected '(' after function name")) return 0;
	ctype_t arg_types[CC_PARAMS];
	uint8_t argc = 0;
	if (c->tok.kind != ')') {
		for (;;) {
			if (argc >= CC_PARAMS) return fail_at(c, CELL_CC_PARSE_ERROR, "user functions accept at most eight arguments");
			expr_t a;
			if (!parse_assignment(c, &a) || !expr_rvalue(c, &a)) return 0;
			arg_types[argc] = a.type;
			uint8_t ar;
			if (!alloc_temp(c, &ar) || !emit(c, CELL_EXEC_OP_ADDRL, ar, 0, 0, (int32_t)((uint32_t)slot * CC_CALL_SLOT_BYTES + (uint32_t)argc * 8u)) || !emit(c, CELL_EXEC_OP_STORE64, 0, ar, a.reg, 0)) return 0;
			free_temp(c, ar);
			free_temp(c, a.reg);
			++argc;
			if (c->tok.kind != ',') break;
			if (!next(c)) return 0;
		}
	}
	if (!expect(c, ')', "expected ')' after function arguments")) return 0;
	--c->call_scratch_depth;
	uint8_t fi;
	if (!find_func(c, name, &fi)) { if (!add_function(c, name, type_int(), arg_types, argc, 0u, &fi)) return 0;
	}
	else {
		function_t *f = &c->funcs[fi];
		if (f->param_count != argc) return fail_at(c, CELL_CC_PARSE_ERROR, "function argument count mismatch");
		for (uint8_t i = 0; i < argc; ++i) if (!type_compatible(f->params[i], arg_types[i], 0)) return fail_at(c, CELL_CC_PARSE_ERROR, "function argument type mismatch");
	}
	uint8_t dst;
	if (!alloc_temp(c, &dst)) return 0;
	uint8_t ar = 0;
	if (argc && (!alloc_temp(c, &ar) || !emit(c, CELL_EXEC_OP_ADDRL, ar, 0, 0, (int32_t)((uint32_t)slot * CC_CALL_SLOT_BYTES)))) return 0;
	function_t *f = &c->funcs[fi];
	uint32_t pc = c->code_count;
	uint32_t target = f->defined ? f->entry_pc : 0u;
	if (!emit(c, CELL_EXEC_OP_CALLN, dst, argc ? ar : 0u, 0, CELL_EXEC_CALL_PACK(target, argc))) return 0;
	if (argc) free_temp(c, ar);
	if (!f->defined) { if (c->call_patch_count >= CC_CALL_PATCHES) return fail_at(c, CELL_CC_TOO_COMPLEX, "too many unresolved function calls");
	c->call_patches[c->call_patch_count].pc = pc;
	c->call_patches[c->call_patch_count].func_index = fi;
	++c->call_patch_count;
	}
	out->reg = dst;
	out->type = f->return_type;
	out->array_len = 0u;
	out->lvalue = 0u;
	out->const_zero = 0u;
	return 1;
}
static int parse_primary(compiler_t *c, expr_t *out) {
	zero_bytes(out, sizeof(*out));
	if (c->tok.kind == TOK_NUMBER) {
		int32_t v = c->tok.number;
		uint8_t r;
		if (!alloc_temp(c, &r) || !next(c) || !emit(c, CELL_EXEC_OP_MOVI, r, 0, 0, v)) return 0;
		out->reg = r;
		out->type = type_int();
		out->const_zero = v == 0;
		return 1;
	}
	if (c->tok.kind == TOK_STRING) {
		uint32_t off;
		uint8_t r;
		if (!add_data(c, c->tok.text, c->tok.text_len, 0, &off) || !next(c) || !alloc_temp(c, &r) || !emit(c, CELL_EXEC_OP_MOVI, r, 0, 0, (int32_t)(CELL_EXEC_DATA_BASE + off))) return 0;
		out->reg = r;
		out->type = type_make(BT_CHAR, 1u, 0u);
		return 1;
	}
	if (c->tok.kind == '(') { if (!next(c) || !parse_expr(c, out) || !expect(c, ')', "expected ')' after expression")) return 0;
	return 1;
	}
	if (c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected expression");
	char name[CC_IDENT_MAX + 1u];
	s_copy(name, sizeof(name), c->tok.text);
	if (!next(c)) return 0;
	if (c->tok.kind == '(') {
		if (sys_builtin_nr_name(name)) return parse_sys_call(c, name, out);
		if (s_eq(name, "strlen") || s_eq(name, "atoi") || s_eq(name, "strcmp") || s_eq(name, "puts") || s_eq(name, "putchar") || s_eq(name, "cell_capability")) return parse_library_call(c, name, out);
		return parse_user_call(c, name, out);
	}
	if (s_eq(name, "errno")) {
		uint8_t r;
		if (!alloc_temp(c, &r) || !emit(c, CELL_EXEC_OP_SYSCALL, r, 0, 0, CELL_EXEC_SYSCALL_PACK(CELL_EXEC_SYS_ERRNO, 0))) return 0;
		out->reg = r;
		out->type = type_int();
		return 1;
	}
	int32_t constant;
	if (named_constant(name, &constant)) {
		uint8_t r;
		if (!alloc_temp(c, &r) || !emit(c, CELL_EXEC_OP_MOVI, r, 0, 0, constant)) return 0;
		out->reg = r;
		out->type = type_int();
		out->const_zero = constant == 0;
		return 1;
	}
	uint8_t r;
	ctype_t type;
	uint32_t array_len;
	if (!emit_symbol_address(c, name, &r, &type, &array_len)) return fail_at(c, CELL_CC_PARSE_ERROR, "unknown variable or function");
	out->reg = r;
	out->type = type;
	out->array_len = array_len;
	out->lvalue = 1u;
	return 1;
}
static int parse_postfix(compiler_t *c, expr_t *out) {
	if (!parse_primary(c, out)) return 0;
	for (;;) {
		if (c->tok.kind == '[') {
			if (!expr_rvalue(c, out) || !type_is_pointer(out->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "indexing requires pointer or array");
			ctype_t elem = out->type;
			--elem.pointers;
			uint32_t scale = type_size(c, elem);
			if (!scale) return fail_at(c, CELL_CC_PARSE_ERROR, "cannot index incomplete type");
			expr_t idx;
			if (!next(c) || !parse_expr(c, &idx) || !expr_rvalue(c, &idx) || !type_is_integer(idx.type) || !expect(c, ']', "expected ']' after index")) return 0;
			if (scale != 1u) { uint8_t sr;
			if (!alloc_temp(c, &sr) || !emit(c, CELL_EXEC_OP_MOVI, sr, 0, 0, (int32_t)scale) || !emit(c, CELL_EXEC_OP_MUL, idx.reg, idx.reg, sr, 0)) return 0;
			free_temp(c, sr);
			}
			if (!emit(c, CELL_EXEC_OP_ADD, out->reg, out->reg, idx.reg, 0)) return 0;
			free_temp(c, idx.reg);
			out->type = elem;
			out->lvalue = 1u;
			out->array_len = 0u;
			out->const_zero = 0u;
			continue;
		}
		if (c->tok.kind == '.' || c->tok.kind == TOK_ARROW) {
			int arrow = c->tok.kind == TOK_ARROW;
			if (!next(c) || c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected struct member name");
			ctype_t st;
			if (arrow) { if (!expr_rvalue(c, out) || out->type.pointers != 1u || out->type.base != BT_STRUCT) return fail_at(c, CELL_CC_PARSE_ERROR, "'->' requires pointer to struct");
			st = out->type;
			--st.pointers;
			out->lvalue = 1u;
			}
			else { if (!out->lvalue || !type_is_struct(out->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "'.' requires struct object");
			st = out->type;
			}
			field_t f;
			if (!find_field(c, st.struct_index, c->tok.text, &f)) return fail_at(c, CELL_CC_PARSE_ERROR, "unknown struct member");
			if (!next(c) || !emit(c, CELL_EXEC_OP_ADDI, out->reg, out->reg, 0, (int32_t)f.offset)) return 0;
			out->type = f.type;
			out->array_len = f.array_len;
			out->lvalue = 1u;
			out->const_zero = 0u;
			continue;
		}
		if (c->tok.kind == TOK_INC || c->tok.kind == TOK_DEC) {
			int inc = c->tok.kind == TOK_INC;
			if (!out->lvalue || out->array_len || type_is_struct(out->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "postfix increment/decrement requires modifiable scalar lvalue");
			uint8_t addr = out->reg, old, nv;
			if (!alloc_temp(c, &old) || !emit_load(c, old, addr, out->type) || !alloc_temp(c, &nv) || !emit(c, CELL_EXEC_OP_MOV, nv, old, 0, 0)) return 0;
			int32_t step = type_is_pointer(out->type) ? (int32_t)type_size(c, type_make(out->type.base, out->type.pointers - 1u, out->type.struct_index)) : 1;
			if (!emit(c, CELL_EXEC_OP_ADDI, nv, nv, 0, inc ? step : -step) || !emit_store(c, addr, nv, out->type) || !next(c)) return 0;
			free_temp(c, nv);
			free_temp(c, addr);
			out->reg = old;
			out->lvalue = 0u;
			out->array_len = 0u;
			continue;
		}
		break;
	}
	return 1;
}
static int parse_unary(compiler_t *c, expr_t *out) {
	if (c->tok.kind == '&') {
		if (!next(c) || !parse_unary(c, out)) return 0;
		if (!out->lvalue) return fail_at(c, CELL_CC_PARSE_ERROR, "unary '&' requires lvalue");
		out->type.pointers++;
		out->array_len = 0u;
		out->lvalue = 0u;
		out->const_zero = 0u;
		return 1;
	}
	if (c->tok.kind == '*') {
		if (!next(c) || !parse_unary(c, out) || !expr_rvalue(c, out) || !type_is_pointer(out->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "unary '*' requires pointer");
		--out->type.pointers;
		out->lvalue = 1u;
		out->array_len = 0u;
		return 1;
	}
	if (c->tok.kind == '+' || c->tok.kind == '-' || c->tok.kind == '!' || c->tok.kind == '~') {
		int op = c->tok.kind;
		if (!next(c) || !parse_unary(c, out) || !expr_rvalue(c, out)) return 0;
		if (op == '!') { if (!type_is_integer(out->type) && !type_is_pointer(out->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "logical not requires scalar");
		uint8_t z;
		if (!alloc_temp(c, &z) || !emit(c, CELL_EXEC_OP_MOVI, z, 0, 0, 0) || !emit(c, CELL_EXEC_OP_CMPEQ, out->reg, out->reg, z, 0)) return 0;
		free_temp(c, z);
		out->type = type_int();
		return 1;
		}
		if (!type_is_integer(out->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "unary arithmetic operator requires integer");
		if (op == '-') { uint8_t z;
		if (!alloc_temp(c, &z) || !emit(c, CELL_EXEC_OP_MOVI, z, 0, 0, 0) || !emit(c, CELL_EXEC_OP_SUB, out->reg, z, out->reg, 0)) return 0;
		free_temp(c, z);
		}
		else if (op == '~') { uint8_t m;
		if (!alloc_temp(c, &m) || !emit(c, CELL_EXEC_OP_MOVI, m, 0, 0, -1) || !emit(c, CELL_EXEC_OP_XOR, out->reg, out->reg, m, 0)) return 0;
		free_temp(c, m);
		}
		out->type = type_int();
		return 1;
	}
	if (c->tok.kind == TOK_INC || c->tok.kind == TOK_DEC) {
		int inc = c->tok.kind == TOK_INC;
		if (!next(c) || !parse_unary(c, out)) return 0;
		if (!out->lvalue || out->array_len || type_is_struct(out->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "prefix increment/decrement requires modifiable scalar lvalue");
		uint8_t addr = out->reg, v;
		if (!alloc_temp(c, &v) || !emit_load(c, v, addr, out->type)) return 0;
		int32_t step = type_is_pointer(out->type) ? (int32_t)type_size(c, type_make(out->type.base, out->type.pointers - 1u, out->type.struct_index)) : 1;
		if (!emit(c, CELL_EXEC_OP_ADDI, v, v, 0, inc ? step : -step) || !emit_store(c, addr, v, out->type)) return 0;
		free_temp(c, addr);
		out->reg = v;
		out->lvalue = 0u;
		out->array_len = 0u;
		return 1;
	}
	if (tok_ident(c, "sizeof")) {
		if (!next(c)) return 0;
		uint32_t size = 0;
		if (c->tok.kind == '(') {
			if (!next(c)) return 0;
			if (tok_ident(c, "int") || tok_ident(c, "char") || tok_ident(c, "void") || tok_ident(c, "struct")) {
				ctype_t t;
				if (!parse_decl_spec(c, &t) || !parse_declarator_type(c, t, &t) || !expect(c, ')', "expected ')' after sizeof type")) return 0;
				size = type_size(c, t);
				if (!size) return fail_at(c, CELL_CC_PARSE_ERROR, "sizeof incomplete type");
			} else {
				expr_t e;
				if (!parse_expr(c, &e) || !expect(c, ')', "expected ')' after sizeof expression")) return 0;
				size = e.array_len ? object_size(c, e.type, e.array_len) : type_size(c, e.type);
				free_temp(c, e.reg);
			}
		} else if (c->tok.kind == TOK_IDENT) {
			uint32_t li, gi;
			if (find_local(c, c->tok.text, &li)) size = object_size(c, c->locals[li].type, c->locals[li].array_len);
			else if (find_global(c, c->tok.text, &gi)) size = object_size(c, c->globals[gi].type, c->globals[gi].array_len);
			else return fail_at(c, CELL_CC_PARSE_ERROR, "sizeof unknown identifier");
			if (!next(c)) return 0;
		} else return fail_at(c, CELL_CC_PARSE_ERROR, "unsupported sizeof operand");
		uint8_t r;
		if (!alloc_temp(c, &r) || !emit(c, CELL_EXEC_OP_MOVI, r, 0, 0, (int32_t)size)) return 0;
		out->reg = r;
		out->type = type_int();
		out->lvalue = 0u;
		out->array_len = 0u;
		out->const_zero = size == 0u;
		return 1;
	}
	return parse_postfix(c, out);
}

static int parse_mul(compiler_t *c, expr_t *out) {
	if (!parse_unary(c, out)) return 0;
	while (c->tok.kind == '*' || c->tok.kind == '/' || c->tok.kind == '%') {
		int op = c->tok.kind;
		if (!expr_rvalue(c, out) || !type_is_integer(out->type) || !next(c)) return fail_at(c, CELL_CC_PARSE_ERROR, "multiplicative operators require integers");
		expr_t r;
		if (!parse_unary(c, &r) || !expr_rvalue(c, &r) || !type_is_integer(r.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "multiplicative operators require integers");
		uint8_t code = op == '*' ? CELL_EXEC_OP_MUL : op == '/' ? CELL_EXEC_OP_DIV : CELL_EXEC_OP_MOD;
		if (!emit(c, code, out->reg, out->reg, r.reg, 0)) return 0;
		free_temp(c, r.reg);
		out->type = type_int();
		out->const_zero = 0u;
	}
	return 1;
}
static int scale_integer_for_pointer(compiler_t *c, expr_t *v, ctype_t pointer_type) {
	ctype_t elem = pointer_type;
	if (!elem.pointers) return 0;
	--elem.pointers;
	uint32_t scale = type_size(c, elem);
	if (!scale) return fail_at(c, CELL_CC_PARSE_ERROR, "pointer arithmetic on incomplete or void type");
	if (scale != 1u) { uint8_t s;
	if (!alloc_temp(c, &s) || !emit(c, CELL_EXEC_OP_MOVI, s, 0, 0, (int32_t)scale) || !emit(c, CELL_EXEC_OP_MUL, v->reg, v->reg, s, 0)) return 0;
	free_temp(c, s);
	} return 1;
}
static int parse_add(compiler_t *c, expr_t *out) {
	if (!parse_mul(c, out)) return 0;
	while (c->tok.kind == '+' || c->tok.kind == '-') {
		int op = c->tok.kind;
		if (!expr_rvalue(c, out) || !next(c)) return 0;
		expr_t r;
		if (!parse_mul(c, &r) || !expr_rvalue(c, &r)) return 0;
		if (type_is_integer(out->type) && type_is_integer(r.type)) {
			if (!emit(c, op == '+' ? CELL_EXEC_OP_ADD : CELL_EXEC_OP_SUB, out->reg, out->reg, r.reg, 0)) return 0;
			out->type = type_int();
		} else if (type_is_pointer(out->type) && type_is_integer(r.type)) {
			if (!scale_integer_for_pointer(c, &r, out->type) || !emit(c, op == '+' ? CELL_EXEC_OP_ADD : CELL_EXEC_OP_SUB, out->reg, out->reg, r.reg, 0)) return 0;
		} else if (op == '+' && type_is_integer(out->type) && type_is_pointer(r.type)) {
			if (!scale_integer_for_pointer(c, out, r.type) || !emit(c, CELL_EXEC_OP_ADD, r.reg, r.reg, out->reg, 0)) return 0;
			free_temp(c, out->reg);
			*out = r;
			out->const_zero = 0u;
			continue;
		} else if (op == '-' && type_is_pointer(out->type) && type_is_pointer(r.type) && type_equal(out->type, r.type)) {
			ctype_t elem = out->type;
			--elem.pointers;
			uint32_t scale = type_size(c, elem);
			if (!scale || !emit(c, CELL_EXEC_OP_SUB, out->reg, out->reg, r.reg, 0)) return 0;
			if (scale != 1u) { uint8_t s;
			if (!alloc_temp(c, &s) || !emit(c, CELL_EXEC_OP_MOVI, s, 0, 0, (int32_t)scale) || !emit(c, CELL_EXEC_OP_DIV, out->reg, out->reg, s, 0)) return 0;
			free_temp(c, s);
			} out->type = type_int();
		} else return fail_at(c, CELL_CC_PARSE_ERROR, "unsupported pointer arithmetic");
		free_temp(c, r.reg);
		out->const_zero = 0u;
	}
	return 1;
}
static int parse_shift(compiler_t *c, expr_t *out) {
	if (!parse_add(c, out)) return 0;
	while (c->tok.kind == TOK_SHL || c->tok.kind == TOK_SHR) {
		int op = c->tok.kind;
		if (!expr_rvalue(c, out) || !type_is_integer(out->type) || !next(c)) return fail_at(c, CELL_CC_PARSE_ERROR, "shift operators require integers");
		expr_t r;
		if (!parse_add(c, &r) || !expr_rvalue(c, &r) || !type_is_integer(r.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "shift operators require integers");
		if (!emit(c, op == TOK_SHL ? CELL_EXEC_OP_SHL : CELL_EXEC_OP_SHR, out->reg, out->reg, r.reg, 0)) return 0;
		free_temp(c, r.reg);
		out->type = type_int();
		out->const_zero = 0u;
	}
	return 1;
}
static int comparison_opcode(int kind) {
	switch (kind) { case TOK_EQ: return CELL_EXEC_OP_CMPEQ;
	case TOK_NE: return CELL_EXEC_OP_CMPNE;
	case '<': return CELL_EXEC_OP_CMPLT;
	case TOK_LE: return CELL_EXEC_OP_CMPLE;
	case '>': return CELL_EXEC_OP_CMPGT;
	case TOK_GE: return CELL_EXEC_OP_CMPGE;
	default: return 0;
	}
}
static int parse_compare(compiler_t *c, expr_t *out) {
	if (!parse_shift(c, out)) return 0;
	while (comparison_opcode(c->tok.kind)) {
		int code = comparison_opcode(c->tok.kind);
		if (!expr_rvalue(c, out) || !next(c)) return 0;
		expr_t r;
		if (!parse_shift(c, &r) || !expr_rvalue(c, &r)) return 0;
		int ok = (type_is_integer(out->type) && type_is_integer(r.type)) || (type_is_pointer(out->type) && type_is_pointer(r.type) && (type_compatible(out->type, r.type, 0) || type_compatible(r.type, out->type, 0))) || (type_is_pointer(out->type) && type_is_integer(r.type) && r.const_zero) || (type_is_integer(out->type) && out->const_zero && type_is_pointer(r.type));
		if (!ok) return fail_at(c, CELL_CC_PARSE_ERROR, "incompatible comparison operands");
		if (!emit(c, (uint8_t)code, out->reg, out->reg, r.reg, 0)) return 0;
		free_temp(c, r.reg);
		out->type = type_int();
		out->const_zero = 0u;
	}
	return 1;
}
static int parse_bitand(compiler_t *c, expr_t *out) {
	if (!parse_compare(c, out)) return 0;
	while (c->tok.kind == '&') { if (!expr_rvalue(c, out) || !type_is_integer(out->type) || !next(c)) return fail_at(c, CELL_CC_PARSE_ERROR, "bitwise '&' requires integers");
	expr_t r;
	if (!parse_compare(c, &r) || !expr_rvalue(c, &r) || !type_is_integer(r.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "bitwise '&' requires integers");
	if (!emit(c, CELL_EXEC_OP_AND, out->reg, out->reg, r.reg, 0)) return 0;
	free_temp(c, r.reg);
	out->type = type_int();
	out->const_zero = 0u;
	} return 1;
}
static int parse_bitxor(compiler_t *c, expr_t *out) {
	if (!parse_bitand(c, out)) return 0;
	while (c->tok.kind == '^') { if (!expr_rvalue(c, out) || !type_is_integer(out->type) || !next(c)) return fail_at(c, CELL_CC_PARSE_ERROR, "bitwise '^' requires integers");
	expr_t r;
	if (!parse_bitand(c, &r) || !expr_rvalue(c, &r) || !type_is_integer(r.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "bitwise '^' requires integers");
	if (!emit(c, CELL_EXEC_OP_XOR, out->reg, out->reg, r.reg, 0)) return 0;
	free_temp(c, r.reg);
	out->type = type_int();
	out->const_zero = 0u;
	} return 1;
}
static int parse_bitor(compiler_t *c, expr_t *out) {
	if (!parse_bitxor(c, out)) return 0;
	while (c->tok.kind == '|') { if (!expr_rvalue(c, out) || !type_is_integer(out->type) || !next(c)) return fail_at(c, CELL_CC_PARSE_ERROR, "bitwise '|' requires integers");
	expr_t r;
	if (!parse_bitxor(c, &r) || !expr_rvalue(c, &r) || !type_is_integer(r.type)) return fail_at(c, CELL_CC_PARSE_ERROR, "bitwise '|' requires integers");
	if (!emit(c, CELL_EXEC_OP_OR, out->reg, out->reg, r.reg, 0)) return 0;
	free_temp(c, r.reg);
	out->type = type_int();
	out->const_zero = 0u;
	} return 1;
}
static int normalize_bool(compiler_t *c, expr_t *e) {
	if (!expr_truth(c, e)) return 0;
	uint8_t z;
	if (!alloc_temp(c, &z) || !emit(c, CELL_EXEC_OP_MOVI, z, 0, 0, 0) || !emit(c, CELL_EXEC_OP_CMPNE, e->reg, e->reg, z, 0)) return 0;
	free_temp(c, z);
	e->type = type_int();
	e->const_zero = 0u;
	return 1;
}
static int parse_logand(compiler_t *c, expr_t *out) {
	if (!parse_bitor(c, out)) return 0;
	while (c->tok.kind == TOK_ANDAND) {
		if (!normalize_bool(c, out)) return 0;
		uint32_t false_pc = c->code_count;
		if (!emit(c, CELL_EXEC_OP_JZ, 0, out->reg, 0, 0) || !next(c)) return 0;
		expr_t r;
		if (!parse_bitor(c, &r) || !normalize_bool(c, &r) || !emit(c, CELL_EXEC_OP_MOV, out->reg, r.reg, 0, 0)) return 0;
		free_temp(c, r.reg);
		if (!patch_branch(c, false_pc, c->code_count)) return 0;
		out->type = type_int();
	}
	return 1;
}
static int parse_logor(compiler_t *c, expr_t *out) {
	if (!parse_logand(c, out)) return 0;
	while (c->tok.kind == TOK_OROR) {
		if (!normalize_bool(c, out)) return 0;
		uint32_t true_pc = c->code_count;
		if (!emit(c, CELL_EXEC_OP_JNZ, 0, out->reg, 0, 0) || !next(c)) return 0;
		expr_t r;
		if (!parse_logand(c, &r) || !normalize_bool(c, &r) || !emit(c, CELL_EXEC_OP_MOV, out->reg, r.reg, 0, 0)) return 0;
		free_temp(c, r.reg);
		if (!patch_branch(c, true_pc, c->code_count)) return 0;
		out->type = type_int();
	}
	return 1;
}
static int compound_value(compiler_t *c, int op, ctype_t lhs_type, uint8_t dst, expr_t *rhs) {
	if (!expr_rvalue(c, rhs)) return 0;
	if ((op == TOK_ADD_EQ || op == TOK_SUB_EQ) && type_is_pointer(lhs_type) && type_is_integer(rhs->type)) {
		if (!scale_integer_for_pointer(c, rhs, lhs_type) || !emit(c, op == TOK_ADD_EQ ? CELL_EXEC_OP_ADD : CELL_EXEC_OP_SUB, dst, dst, rhs->reg, 0)) return 0;
		return 1;
	}
	if (!type_is_integer(lhs_type) || !type_is_integer(rhs->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "compound assignment requires integer operands or pointer +/- integer");
	uint8_t code = op == TOK_ADD_EQ ? CELL_EXEC_OP_ADD : op == TOK_SUB_EQ ? CELL_EXEC_OP_SUB : op == TOK_MUL_EQ ? CELL_EXEC_OP_MUL : op == TOK_DIV_EQ ? CELL_EXEC_OP_DIV : CELL_EXEC_OP_MOD;
	return emit(c, code, dst, dst, rhs->reg, 0);
}
static int parse_assignment(compiler_t *c, expr_t *out) {
	if (!parse_logor(c, out)) return 0;
	int op = c->tok.kind;
	if (op != '=' && op != TOK_ADD_EQ && op != TOK_SUB_EQ && op != TOK_MUL_EQ && op != TOK_DIV_EQ && op != TOK_MOD_EQ) return 1;
	if (!out->lvalue || out->array_len || type_is_struct(out->type)) return fail_at(c, CELL_CC_PARSE_ERROR, "assignment requires modifiable scalar lvalue");
	uint8_t addr = out->reg;
	ctype_t lhs_type = out->type;
	if (!next(c)) return 0;
	expr_t rhs;
	if (!parse_assignment(c, &rhs)) return 0;
	if (op == '=') {
		if (!expr_rvalue(c, &rhs) || !type_compatible(lhs_type, rhs.type, rhs.const_zero)) return fail_at(c, CELL_CC_PARSE_ERROR, "assignment type mismatch");
		if (!emit_store(c, addr, rhs.reg, lhs_type)) return 0;
		free_temp(c, addr);
		*out = rhs;
		out->type = lhs_type;
		out->const_zero = rhs.const_zero;
		return 1;
	}
	uint8_t old;
	if (!alloc_temp(c, &old) || !emit_load(c, old, addr, lhs_type) || !compound_value(c, op, lhs_type, old, &rhs) || !emit_store(c, addr, old, lhs_type)) return 0;
	free_temp(c, rhs.reg);
	free_temp(c, addr);
	out->reg = old;
	out->type = lhs_type;
	out->array_len = 0u;
	out->lvalue = 0u;
	out->const_zero = 0u;
	return 1;
}
static int parse_expr(compiler_t *c, expr_t *out) { return parse_assignment(c, out);
}

static int parse_statement(compiler_t *c);
static int parse_block(compiler_t *c) {
	if (!expect(c, '{', "expected '{'")) return 0;
	uint32_t saved_count = c->local_count;
	++c->scope_depth;
	while (c->tok.kind != '}' && c->tok.kind != TOK_EOF) if (!parse_statement(c)) return 0;
	if (!expect(c, '}', "expected '}'")) return 0;
	c->local_count = saved_count;
	--c->scope_depth;
	return 1;
}
static int emit_local_element_address(compiler_t *c, const local_t *v, uint32_t byte_off, uint8_t *addr) {
	return alloc_temp(c, addr) && emit(c, CELL_EXEC_OP_ADDRL, *addr, 0, 0, (int32_t)(v->offset + byte_off));
}
static int parse_local_array_initializer(compiler_t *c, local_t *v) {
	uint32_t elem_size = type_size(c, v->type);
	if (!elem_size) return fail_at(c, CELL_CC_PARSE_ERROR, "invalid array element type");
	if (v->type.base == BT_CHAR && v->type.pointers == 0u && c->tok.kind == TOK_STRING) {
		uint32_t need = c->tok.text_len + 1u;
		if (need > v->array_len) return fail_at(c, CELL_CC_PARSE_ERROR, "string initializer is too long for char array");
		for (uint32_t i = 0; i < need; ++i) { uint8_t a, x;
		int32_t ch = i < c->tok.text_len ? (unsigned char)c->tok.text[i] : 0;
		if (!emit_local_element_address(c, v, i, &a) || !alloc_temp(c, &x) || !emit(c, CELL_EXEC_OP_MOVI, x, 0, 0, ch) || !emit(c, CELL_EXEC_OP_STORE8, 0, a, x, 0)) return 0;
		free_temp(c, x);
		free_temp(c, a);
		}
		return next(c);
	}
	if (c->tok.kind != '{') return fail_at(c, CELL_CC_PARSE_ERROR, "array initializer requires string or brace list");
	if (!next(c)) return 0;
	uint32_t i = 0;
	while (c->tok.kind != '}') {
		if (i >= v->array_len) return fail_at(c, CELL_CC_PARSE_ERROR, "too many array initializer elements");
		expr_t e;
		if (!parse_assignment(c, &e) || !expr_rvalue(c, &e) || !type_compatible(v->type, e.type, e.const_zero)) return fail_at(c, CELL_CC_PARSE_ERROR, "array initializer type mismatch");
		uint8_t a;
		if (!emit_local_element_address(c, v, i * elem_size, &a) || !emit_store(c, a, e.reg, v->type)) return 0;
		free_temp(c, a);
		free_temp(c, e.reg);
		++i;
		if (c->tok.kind == ',') { if (!next(c)) return 0;
		if (c->tok.kind == '}') break;
		} else break;
	}
	return expect(c, '}', "expected '}' after array initializer");
}
static int parse_local_declaration(compiler_t *c) {
	ctype_t base;
	if (!parse_decl_spec(c, &base)) return 0;
	for (;;) {
		ctype_t type;
		if (!parse_declarator_type(c, base, &type) || c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected variable name");
		char name[CC_IDENT_MAX + 1u];
		s_copy(name, sizeof(name), c->tok.text);
		if (!next(c)) return 0;
		uint32_t array_len;
		if (!parse_array_suffix(c, &array_len)) return 0;
		if (type.base == BT_VOID && !type.pointers) return fail_at(c, CELL_CC_PARSE_ERROR, "object cannot have void type");
		if (array_len == CC_ARRAY_UNSIZED) return fail_at(c, CELL_CC_PARSE_ERROR, "local arrays require an explicit bound");
		uint32_t li;
		if (!add_local(c, name, type, array_len, &li)) return 0;
		local_t *v = &c->locals[li];
		if (c->tok.kind == '=') {
			if (!next(c)) return 0;
			if (array_len) { if (!parse_local_array_initializer(c, v)) return 0;
			}
			else if (type_is_struct(type)) return fail_at(c, CELL_CC_PARSE_ERROR, "struct aggregate initializer is not yet part of the bounded subset; initialize members explicitly");
			else { expr_t e;
			if (!parse_assignment(c, &e) || !expr_rvalue(c, &e) || !type_compatible(type, e.type, e.const_zero)) return fail_at(c, CELL_CC_PARSE_ERROR, "initializer type mismatch");
			uint8_t a;
			if (!emit_local_element_address(c, v, 0u, &a) || !emit_store(c, a, e.reg, type)) return 0;
			free_temp(c, a);
			free_temp(c, e.reg);
			}
		}
		if (c->tok.kind != ',') break;
		if (!next(c)) return 0;
	}
	return expect(c, ';', "expected ';' after declaration");
}
static int parse_if(compiler_t *c) {
	if (!next(c) || !expect(c, '(', "expected '(' after if")) return 0;
	expr_t cond;
	if (!parse_expr(c, &cond) || !expr_truth(c, &cond) || !expect(c, ')', "expected ')' after if condition")) return 0;
	uint32_t false_pc = c->code_count;
	if (!emit(c, CELL_EXEC_OP_JZ, 0, cond.reg, 0, 0)) return 0;
	free_temp(c, cond.reg);
	if (!parse_statement(c)) return 0;
	if (tok_ident(c, "else")) { uint32_t end_pc = c->code_count;
	if (!emit(c, CELL_EXEC_OP_JMP, 0, 0, 0, 0) || !patch_branch(c, false_pc, c->code_count) || !next(c) || !parse_statement(c) || !patch_branch(c, end_pc, c->code_count)) return 0;
	}
	else if (!patch_branch(c, false_pc, c->code_count)) return 0;
	return 1;
}
static int loop_push(compiler_t *c, uint32_t continue_pc) {
	if (c->loop_depth >= 8u) return fail_at(c, CELL_CC_TOO_COMPLEX, "loop nesting exceeds compiler limit");
	loop_t *l = &c->loops[c->loop_depth++];
	zero_bytes(l, sizeof(*l));
	l->continue_pc = continue_pc;
	return 1;
}
static int loop_pop_patch(compiler_t *c, uint32_t exit_pc) {
	if (!c->loop_depth) return 0;
	loop_t *l = &c->loops[c->loop_depth - 1u];
	for (uint8_t i = 0; i < l->break_count; ++i) if (!patch_branch(c, l->break_patches[i], exit_pc)) return 0;
	--c->loop_depth;
	return 1;
}
static int parse_while(compiler_t *c) {
	if (!next(c) || !expect(c, '(', "expected '(' after while")) return 0;
	uint32_t cond_pc = c->code_count;
	expr_t cond;
	if (!parse_expr(c, &cond) || !expr_truth(c, &cond) || !expect(c, ')', "expected ')' after while condition")) return 0;
	uint32_t exit_branch = c->code_count;
	if (!emit(c, CELL_EXEC_OP_JZ, 0, cond.reg, 0, 0)) return 0;
	free_temp(c, cond.reg);
	if (!loop_push(c, cond_pc) || !parse_statement(c) || !emit_jump_to(c, cond_pc)) return 0;
	uint32_t exit_pc = c->code_count;
	if (!patch_branch(c, exit_branch, exit_pc) || !loop_pop_patch(c, exit_pc)) return 0;
	return 1;
}
static int parse_for(compiler_t *c) {
	if (!next(c) || !expect(c, '(', "expected '(' after for")) return 0;
	uint32_t saved_count = c->local_count;
	++c->scope_depth;
	if (tok_ident(c, "int") || tok_ident(c, "char") || tok_ident(c, "struct") || tok_ident(c, "void")) { if (!parse_local_declaration(c)) return 0;
	}
	else if (c->tok.kind == ';') { if (!next(c)) return 0;
	}
	else { expr_t init;
	if (!parse_expr(c, &init) || !expr_rvalue(c, &init) || !expect(c, ';', "expected ';' after for initializer")) return 0;
	free_temp(c, init.reg);
	}
	uint32_t cond_pc = c->code_count, exit_branch = 0xffffffffu;
	if (c->tok.kind != ';') { expr_t cond;
	if (!parse_expr(c, &cond) || !expr_truth(c, &cond)) return 0;
	exit_branch = c->code_count;
	if (!emit(c, CELL_EXEC_OP_JZ, 0, cond.reg, 0, 0)) return 0;
	free_temp(c, cond.reg);
	}
	if (!expect(c, ';', "expected ';' after for condition")) return 0;
	uint32_t to_body = c->code_count;
	if (!emit(c, CELL_EXEC_OP_JMP, 0, 0, 0, 0)) return 0;
	uint32_t step_pc = c->code_count;
	if (c->tok.kind != ')') { expr_t step;
	if (!parse_expr(c, &step) || !expr_rvalue(c, &step)) return 0;
	free_temp(c, step.reg);
	}
	if (!expect(c, ')', "expected ')' after for clauses") || !emit_jump_to(c, cond_pc)) return 0;
	uint32_t body_pc = c->code_count;
	if (!patch_branch(c, to_body, body_pc) || !loop_push(c, step_pc) || !parse_statement(c) || !emit_jump_to(c, step_pc)) return 0;
	uint32_t exit_pc = c->code_count;
	if (exit_branch != 0xffffffffu && !patch_branch(c, exit_branch, exit_pc)) return 0;
	if (!loop_pop_patch(c, exit_pc)) return 0;
	c->local_count = saved_count;
	--c->scope_depth;
	return 1;
}
static int parse_return(compiler_t *c) {
	function_t *f = &c->funcs[c->current_func];
	if (!next(c)) return 0;
	if (c->tok.kind == ';') { if (f->return_type.base != BT_VOID || f->return_type.pointers) return fail_at(c, CELL_CC_PARSE_ERROR, "non-void function requires return value");
	if (!next(c)) return 0;
	return emit(c, CELL_EXEC_OP_MOVI, CC_TEMP_LAST, 0, 0, 0) && emit(c, CELL_EXEC_OP_RET, 0, CC_TEMP_LAST, 0, 0);
	}
	expr_t e;
	if (!parse_expr(c, &e) || !expr_rvalue(c, &e) || !type_compatible(f->return_type, e.type, e.const_zero) || !expect(c, ';', "expected ';' after return")) return fail_at(c, CELL_CC_PARSE_ERROR, "return type mismatch");
	int ok = emit(c, CELL_EXEC_OP_RET, 0, e.reg, 0, 0);
	free_temp(c, e.reg);
	return ok;
}
static int parse_statement(compiler_t *c) {
	if (c->tok.kind == ';') return next(c);
	if (c->tok.kind == '{') return parse_block(c);
	if (tok_ident(c, "int") || tok_ident(c, "char") || tok_ident(c, "void") || tok_ident(c, "struct")) return parse_local_declaration(c);
	if (tok_ident(c, "if")) return parse_if(c);
	if (tok_ident(c, "while")) return parse_while(c);
	if (tok_ident(c, "for")) return parse_for(c);
	if (tok_ident(c, "return")) return parse_return(c);
	if (tok_ident(c, "break")) {
		if (!c->loop_depth) return fail_at(c, CELL_CC_PARSE_ERROR, "break outside loop");
		loop_t *l = &c->loops[c->loop_depth - 1u];
		if (l->break_count >= 32u) return fail_at(c, CELL_CC_TOO_COMPLEX, "too many break statements in loop");
		if (!next(c) || !expect(c, ';', "expected ';' after break")) return 0;
		l->break_patches[l->break_count++] = c->code_count;
		return emit(c, CELL_EXEC_OP_JMP, 0, 0, 0, 0);
	}
	if (tok_ident(c, "continue")) {
		if (!c->loop_depth) return fail_at(c, CELL_CC_PARSE_ERROR, "continue outside loop");
		uint32_t target = c->loops[c->loop_depth - 1u].continue_pc;
		if (!next(c) || !expect(c, ';', "expected ';' after continue")) return 0;
		return emit_jump_to(c, target);
	}
	expr_t e;
	if (!parse_expr(c, &e) || !expr_rvalue(c, &e) || !expect(c, ';', "expected ';' after expression")) return 0;
	free_temp(c, e.reg);
	return 1;
}

static int parse_struct_definition_after_tag(compiler_t *c, const char *tag) {
	uint8_t existing;
	if (find_struct(c, tag, &existing)) return fail_at(c, CELL_CC_PARSE_ERROR, "duplicate struct definition");
	if (c->struct_count >= CC_STRUCTS) return fail_at(c, CELL_CC_TOO_COMPLEX, "too many struct types");
	uint8_t si = (uint8_t)c->struct_count++;
	struct_type_t *s = &c->structs[si];
	zero_bytes(s, sizeof(*s));
	s_copy(s->name, sizeof(s->name), tag);
	s->align = 1u;
	if (!expect(c, '{', "expected '{' in struct definition")) return 0;
	while (c->tok.kind != '}') {
		ctype_t base;
		if (!parse_decl_spec(c, &base)) return 0;
		for (;;) {
			ctype_t type;
			if (!parse_declarator_type(c, base, &type) || c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected struct field name");
			if (s->field_count >= CC_FIELDS) return fail_at(c, CELL_CC_TOO_COMPLEX, "too many fields in struct");
			field_t *f = &s->fields[s->field_count++];
			zero_bytes(f, sizeof(*f));
			s_copy(f->name, sizeof(f->name), c->tok.text);
			f->type = type;
			if (!next(c) || !parse_array_suffix(c, &f->array_len)) return 0;
			if (f->array_len == CC_ARRAY_UNSIZED) return fail_at(c, CELL_CC_PARSE_ERROR, "struct field array requires explicit bound");
			uint32_t size = object_size(c, f->type, f->array_len), align = type_align(c, f->type);
			if (!size) return fail_at(c, CELL_CC_PARSE_ERROR, "invalid struct field type");
			s->size = align_up(s->size, align);
			f->offset = s->size;
			s->size += size;
			if (align > s->align) s->align = align;
			if (c->tok.kind != ',') break;
			if (!next(c)) return 0;
		}
		if (!expect(c, ';', "expected ';' after struct field declaration")) return 0;
	}
	if (!next(c) || !expect(c, ';', "expected ';' after struct definition")) return 0;
	s->size = align_up(s->size, s->align);
	if (!s->size) return fail_at(c, CELL_CC_PARSE_ERROR, "empty struct is not supported");
	return 1;
}
static int parse_global_constant(compiler_t *c, ctype_t type, uint64_t *value) {
	int neg = 0;
	if (c->tok.kind == '-') { neg = 1;
	if (!next(c)) return 0;
	}
	if (c->tok.kind == TOK_NUMBER) { int64_t v = c->tok.number;
	if (neg) v = -v;
	*value = (uint64_t)v;
	return next(c);
	}
	if (!neg && c->tok.kind == TOK_STRING && type_is_pointer(type) && type.base == BT_CHAR) { uint32_t off;
	if (!add_data(c, c->tok.text, c->tok.text_len, 0, &off)) return 0;
	*value = CELL_EXEC_DATA_BASE + off;
	return next(c);
	}
	if (!neg && c->tok.kind == TOK_IDENT) { int32_t v;
	if (!named_constant(c->tok.text, &v)) return fail_at(c, CELL_CC_PARSE_ERROR, "global initializer must be a constant");
	*value = (uint64_t)(int64_t)v;
	return next(c);
	}
	return fail_at(c, CELL_CC_PARSE_ERROR, "global initializer must be integer constant, named constant, or string pointer");
}
static int parse_global_initializer(compiler_t *c, global_t *g) {
	uint32_t elem_size = type_size(c, g->type);
	if (!g->array_len) { uint64_t v;
	if (!parse_global_constant(c, g->type, &v)) return 0;
	return add_init(c, g->offset, (uint8_t)elem_size, v);
	}
	if (g->type.base == BT_CHAR && g->type.pointers == 0u && c->tok.kind == TOK_STRING) {
		uint32_t need = c->tok.text_len + 1u;
		if (need > g->array_len) return fail_at(c, CELL_CC_PARSE_ERROR, "string initializer is too long for global char array");
		for (uint32_t i = 0; i < need; ++i) { uint64_t v = i < c->tok.text_len ? (unsigned char)c->tok.text[i] : 0u;
		if (v && !add_init(c, g->offset + i, 1u, v)) return 0;
		} return next(c);
	}
	if (c->tok.kind != '{') return fail_at(c, CELL_CC_PARSE_ERROR, "global array initializer requires string or constant brace list");
	if (!next(c)) return 0;
	uint32_t i = 0;
	while (c->tok.kind != '}') { if (i >= g->array_len) return fail_at(c, CELL_CC_PARSE_ERROR, "too many global array initializer elements");
	uint64_t v;
	if (!parse_global_constant(c, g->type, &v)) return 0;
	if (v && !add_init(c, g->offset + i * elem_size, (uint8_t)elem_size, v)) return 0;
	++i;
	if (c->tok.kind == ',') { if (!next(c)) return 0;
	if (c->tok.kind == '}') break;
	} else break;
	}
	return expect(c, '}', "expected '}' after global initializer");
}

static int parse_parameters(compiler_t *c, ctype_t types[CC_PARAMS], char names[CC_PARAMS][CC_IDENT_MAX + 1u], uint8_t *argc) {
	*argc = 0u;
	if (c->tok.kind == ')') return 1;
	if (tok_ident(c, "void")) {
		if (!next(c)) return 0;
		if (c->tok.kind == ')') return 1;
		ctype_t base = type_void(), type;
		if (!parse_declarator_type(c, base, &type) || !type.pointers) return fail_at(c, CELL_CC_PARSE_ERROR, "void parameter must be the sole void marker or a pointer");
		if (*argc >= CC_PARAMS) return fail_at(c, CELL_CC_PARSE_ERROR, "too many function parameters");
		types[*argc] = type;
		names[*argc][0] = 0;
		if (c->tok.kind == TOK_IDENT) { s_copy(names[*argc], CC_IDENT_MAX + 1u, c->tok.text);
		if (!next(c)) return 0;
		} ++*argc;
	} else {
		ctype_t base, type;
		if (!parse_decl_spec(c, &base) || !parse_declarator_type(c, base, &type)) return 0;
		if (*argc >= CC_PARAMS) return fail_at(c, CELL_CC_PARSE_ERROR, "too many function parameters");
		types[*argc] = type;
		names[*argc][0] = 0;
		if (c->tok.kind == TOK_IDENT) { s_copy(names[*argc], CC_IDENT_MAX + 1u, c->tok.text);
		if (!next(c)) return 0;
		} uint32_t arr;
		if (!parse_array_suffix(c, &arr)) return 0;
		if (arr) types[*argc].pointers++;
		++*argc;
	}
	while (c->tok.kind == ',') {
		if (!next(c)) return 0;
		if (*argc >= CC_PARAMS) return fail_at(c, CELL_CC_PARSE_ERROR, "user functions accept at most eight parameters");
		ctype_t base, type;
		if (!parse_decl_spec(c, &base) || !parse_declarator_type(c, base, &type)) return 0;
		types[*argc] = type;
		names[*argc][0] = 0;
		if (c->tok.kind == TOK_IDENT) { s_copy(names[*argc], CC_IDENT_MAX + 1u, c->tok.text);
		if (!next(c)) return 0;
		}
		uint32_t arr;
		if (!parse_array_suffix(c, &arr)) return 0;
		if (arr) types[*argc].pointers++;
		++*argc;
	}
	return 1;
}
static int begin_function_definition(compiler_t *c, uint8_t fi, char names[CC_PARAMS][CC_IDENT_MAX + 1u]) {
	function_t *f = &c->funcs[fi];
	if (f->defined) return fail_at(c, CELL_CC_PARSE_ERROR, "duplicate function definition");
	if (type_is_struct(f->return_type)) return fail_at(c, CELL_CC_PARSE_ERROR, "struct return by value is outside the bounded ABI");
	for (uint8_t i = 0; i < f->param_count; ++i) { if (!names[i][0]) return fail_at(c, CELL_CC_PARSE_ERROR, "function definition requires parameter names");
	if (type_is_struct(f->params[i])) return fail_at(c, CELL_CC_PARSE_ERROR, "struct parameters by value are outside the bounded ABI");
	}
	f->defined = 1u;
	f->entry_pc = c->code_count;
	if (!patch_function_calls(c, fi)) return 0;
	if (s_eq(f->name, "main")) {
		if (c->main_seen) return fail_at(c, CELL_CC_PARSE_ERROR, "duplicate main definition");
		ctype_t cp = type_make(BT_CHAR, 2u, 0u);
		if (f->return_type.base != BT_INT || f->return_type.pointers || !(f->param_count == 0u || (f->param_count == 2u && type_equal(f->params[0], type_int()) && type_equal(f->params[1], cp)))) return fail_at(c, CELL_CC_PARSE_ERROR, "main must be int main(void) or int main(int argc, char **argv)");
		c->main_seen = 1u;
	}
	c->in_function = 1u;
	c->current_func = fi;
	c->local_count = 0u;
	c->local_next = CC_CALL_SCRATCH_BYTES;
	c->scope_depth = 0u;
	c->temp_mask = 0u;
	c->call_scratch_depth = 0u;
	c->loop_depth = 0u;
	c->frame_pc = c->code_count;
	if (!emit(c, CELL_EXEC_OP_FRAME, 0, 0, 0, 0)) return 0;
	for (uint8_t i = 0; i < f->param_count; ++i) {
		uint32_t li;
		if (!add_local(c, names[i], f->params[i], 0u, &li)) return 0;
		uint8_t a;
		if (!emit_local_element_address(c, &c->locals[li], 0u, &a) || !emit_store(c, a, i, f->params[i])) return 0;
		free_temp(c, a);
	}
	if (!parse_block(c)) return 0;
	if (!emit(c, CELL_EXEC_OP_MOVI, CC_TEMP_LAST, 0, 0, 0) || !emit(c, CELL_EXEC_OP_RET, 0, CC_TEMP_LAST, 0, 0)) return 0;
	uint32_t frame_size = align_up(c->local_next, 8u);
	if (frame_size > CC_GLOBAL_BASE) return fail_at(c, CELL_CC_TOO_COMPLEX, "function frame exceeds dynamic memory region");
	c->code[c->frame_pc].imm = (int32_t)frame_size;
	c->in_function = 0u;
	c->local_count = 0u;
	c->local_next = 0u;
	c->scope_depth = 0u;
	c->temp_mask = 0u;
	c->call_scratch_depth = 0u;
	return 1;
}
static int external_function(compiler_t *c, const char *name, ctype_t ret) {
	ctype_t params[CC_PARAMS];
	char names[CC_PARAMS][CC_IDENT_MAX + 1u];
	uint8_t argc;
	zero_bytes(params, sizeof(params));
	zero_bytes(names, sizeof(names));
	if (!expect(c, '(', "expected '(' after function name") || !parse_parameters(c, params, names, &argc) || !expect(c, ')', "expected ')' after function parameters")) return 0;
	uint8_t fi;
	if (!add_function(c, name, ret, params, argc, 1u, &fi)) return 0;
	if (c->tok.kind == ';') return next(c);
	if (c->tok.kind != '{') return fail_at(c, CELL_CC_PARSE_ERROR, "expected ';' or function body");
	return begin_function_definition(c, fi, names);
}
static int external_global_one(compiler_t *c, const char *name, ctype_t type, uint32_t array_len, int is_extern) {
	if (type.base == BT_VOID && !type.pointers) return fail_at(c, CELL_CC_PARSE_ERROR, "global object cannot have void type");
	int has_init = c->tok.kind == '=';
	if (array_len == CC_ARRAY_UNSIZED && !is_extern) {
		if (!has_init || type.base != BT_CHAR || type.pointers) return fail_at(c, CELL_CC_PARSE_ERROR, "unsized global array requires char string initializer");
		if (!next(c) || c->tok.kind != TOK_STRING) return fail_at(c, CELL_CC_PARSE_ERROR, "unsized char array requires string initializer");
		array_len = c->tok.text_len + 1u;
		has_init = 2;
	}
	uint32_t gi;
	if (!add_global_placeholder(c, name, type, array_len, &gi)) return 0;
	if (is_extern) { if (has_init) return fail_at(c, CELL_CC_PARSE_ERROR, "extern object cannot have initializer in this linking model");
	return 1;
	}
	if (!define_global(c, gi, array_len)) return 0;
	global_t *g = &c->globals[gi];
	if (has_init == 1) { if (!next(c)) return 0;
	return parse_global_initializer(c, g);
	}
	if (has_init == 2) return parse_global_initializer(c, g);
	return 1;
}
static int parse_external_after_base(compiler_t *c, ctype_t base, int is_extern) {
	ctype_t type;
	if (!parse_declarator_type(c, base, &type) || c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected external declaration name");
	char name[CC_IDENT_MAX + 1u];
	s_copy(name, sizeof(name), c->tok.text);
	if (!next(c)) return 0;
	if (c->tok.kind == '(') { if (is_extern) { /* Function declarations are external by default. */ } return external_function(c, name, type);
	}
	for (;;) {
		uint32_t array_len;
		if (!parse_array_suffix(c, &array_len) || !external_global_one(c, name, type, array_len, is_extern)) return 0;
		if (c->tok.kind != ',') break;
		if (!next(c)) return 0;
		if (!parse_declarator_type(c, base, &type) || c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected global variable name");
		s_copy(name, sizeof(name), c->tok.text);
		if (!next(c)) return 0;
	}
	return expect(c, ';', "expected ';' after global declaration");
}
static int parse_external(compiler_t *c) {
	int is_extern = 0;
	if (tok_ident(c, "extern")) { is_extern = 1;
	if (!next(c)) return 0;
	}
	if (tok_ident(c, "struct")) {
		if (!next(c) || c->tok.kind != TOK_IDENT) return fail_at(c, CELL_CC_PARSE_ERROR, "expected struct tag");
		char tag[CC_IDENT_MAX + 1u];
		s_copy(tag, sizeof(tag), c->tok.text);
		if (!next(c)) return 0;
		if (c->tok.kind == '{') { if (is_extern) return fail_at(c, CELL_CC_PARSE_ERROR, "extern struct definition is invalid");
		return parse_struct_definition_after_tag(c, tag);
		}
		uint8_t si;
		if (!find_struct(c, tag, &si)) return fail_at(c, CELL_CC_PARSE_ERROR, "unknown struct type");
		return parse_external_after_base(c, type_make(BT_STRUCT, 0u, si), is_extern);
	}
	ctype_t base;
	if (!parse_decl_spec(c, &base)) return 0;
	return parse_external_after_base(c, base, is_extern);
}
static int parse_translation_unit(compiler_t *c) { while (c->tok.kind != TOK_EOF) if (!parse_external(c)) return 0;
return 1;
}
static int emit_global_initializers_and_entry(compiler_t *c) {
	uint8_t main_index;
	if (!find_func(c, "main", &main_index) || !c->funcs[main_index].defined || !c->main_seen) return fail_at(c, CELL_CC_PARSE_ERROR, "linked program requires main definition");
	for (uint32_t i = 0; i < c->call_patch_count; ++i) if (!c->funcs[c->call_patches[i].func_index].defined) return fail_at(c, CELL_CC_PARSE_ERROR, "called function is not defined");
	for (uint32_t i = 0; i < c->addr_patch_count; ++i) if (!c->globals[c->addr_patches[i].global_index].defined) return fail_at(c, CELL_CC_PARSE_ERROR, "referenced extern global is not defined");
	c->entry_pc = c->code_count;
	for (uint32_t i = 0; i < c->init_count; ++i) {
		init_op_t *op = &c->inits[i];
		if (!op->value) continue;
		uint8_t store = op->size == 1u ? CELL_EXEC_OP_STORE8 : op->size == 4u ? CELL_EXEC_OP_STORE32 : op->size == 8u ? CELL_EXEC_OP_STORE64 : 0u;
		if (!store) return fail_at(c, CELL_CC_TOO_COMPLEX, "invalid global initializer width");
		if (!emit(c, CELL_EXEC_OP_MOVI, 8u, 0, 0, (int32_t)(CC_GLOBAL_BASE + op->offset)) || !emit(c, CELL_EXEC_OP_MOVI, 9u, 0, 0, (int32_t)op->value) || !emit(c, store, 0, 8u, 9u, 0)) return 0;
	}
	function_t *m = &c->funcs[main_index];
	uint8_t argc = m->param_count;
	if (!emit(c, CELL_EXEC_OP_CALL, 8u, argc >= 1u ? 0u : 0u, argc >= 2u ? 1u : 0u, CELL_EXEC_CALL_PACK(m->entry_pc, argc)) || !emit(c, CELL_EXEC_OP_EXIT, 0, 8u, 0, 0)) return 0;
	return 1;
}
int cell_cc_compile_units(const cell_cc_source_unit_t *units, size_t unit_count,
	void *output, size_t output_cap, size_t *output_bytes, cell_cc_diag_t *diag) {
	if (output_bytes) *output_bytes = 0u;
	if (diag) { zero_bytes(diag, sizeof(*diag));
	diag->status = CELL_CC_OK;
	}
	if (!units || !unit_count || unit_count > CELL_CC_UNIT_MAX || !output || !output_bytes || !diag) { if (diag) diag_set(diag, CELL_CC_BAD_ARGUMENT, 1u, 1u, "invalid compiler arguments");
	return 0;
	}
	compiler_t *c = &g_cc;
	zero_bytes(c, sizeof(*c));
	c->diag = diag;
	for (size_t u = 0; u < unit_count; ++u) {
		if (!units[u].source || !units[u].source_bytes || units[u].source_bytes > CELL_CC_SOURCE_MAX) { diag_set(diag, CELL_CC_SOURCE_TOO_LARGE, 1u, 1u, "source file exceeds compiler limit");
		return 0;
		}
		if (!preprocess(c, units[u].source, units[u].source_bytes) || c->failed) return 0;
		zero_bytes(&c->lex, sizeof(c->lex));
		c->lex.src = c->preprocessed;
		c->lex.bytes = s_len(c->preprocessed);
		c->lex.line = 1u;
		c->lex.column = 1u;
		c->lex.diag = diag;
		if (!next(c) || !parse_translation_unit(c) || c->failed) return 0;
	}
	if (!emit_global_initializers_and_entry(c) || c->failed) return 0;
	uint64_t code_bytes = (uint64_t)c->code_count * CELL_EXEC_INSN_BYTES, payload = code_bytes + c->data_bytes, total = CELL_EXEC_HEADER_BYTES + payload;
	if (total > output_cap || total > CELL_EXEC_FILE_MAX) { diag_set(diag, CELL_CC_OUTPUT_TOO_LARGE, 1u, 1u, "compiled program exceeds CellExec-1 file limit");
	return 0;
	}
	uint8_t *dst = (uint8_t *)output;
	zero_bytes(dst, (size_t)total);
	cell_exec_header_t *h = (cell_exec_header_t *)dst;
	h->magic = CELL_EXEC_MAGIC;
	h->version = CELL_EXEC_VERSION;
	h->header_bytes = CELL_EXEC_HEADER_BYTES;
	h->instruction_bytes = CELL_EXEC_INSN_BYTES;
	h->code_bytes = (uint32_t)code_bytes;
	h->data_bytes = c->data_bytes;
	h->entry_pc = c->entry_pc;
	h->capability_mask = c->capability_mask;
	h->memory_bytes = CELL_CC_TASK_MEMORY;
	uint64_t gas = (uint64_t)c->code_count * 4096u + 4096u;
	if (gas > CELL_EXEC_GAS_MAX) gas = CELL_EXEC_GAS_MAX;
	h->gas_limit = (uint32_t)gas;
	h->flags = CELL_EXEC_F_STATIC_MEMORY;
	h->reserved0 = CELL_CC_STATIC_MEMORY;
	h->total_bytes = (uint32_t)total;
	cell_exec_insn_t *code = (cell_exec_insn_t *)(dst + CELL_EXEC_HEADER_BYTES);
	for (uint32_t i = 0; i < c->code_count; ++i) code[i] = c->code[i];
	uint8_t *data_dst = dst + CELL_EXEC_HEADER_BYTES + code_bytes;
	for (uint32_t i = 0; i < c->data_bytes; ++i) data_dst[i] = c->data[i];
	h->payload_crc32 = cell_exec_crc32(dst + CELL_EXEC_HEADER_BYTES, (size_t)payload);
	*output_bytes = (size_t)total;
	return 1;
}
int cell_cc_compile(const char *source, size_t source_bytes,
	void *output, size_t output_cap, size_t *output_bytes, cell_cc_diag_t *diag) {
	cell_cc_source_unit_t unit;
	unit.name = "<input>";
	unit.source = source;
	unit.source_bytes = source_bytes;
	return cell_cc_compile_units(&unit, 1u, output, output_cap, output_bytes, diag);
}
