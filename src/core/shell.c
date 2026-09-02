/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/shell.h"
#include "core/capability.h"
#include "core/cc.h"
#include "core/task.h"
#include "core/vfs.h"

#define CELL_SHELL_MAX_ARGS 16u
#define CELL_SHELL_BUFFER 512u

static char cc_source[CELL_CC_SOURCE_MAX];
static uint8_t cc_image[CELL_EXEC_FILE_MAX];

typedef struct {
	char storage[CELL_SHELL_BUFFER];
	char *argv[CELL_SHELL_MAX_ARGS];
	uint32_t argc;
	char *redirect;
	int append;
	int syntax_error;
	int unsupported_operator;
} shell_words_t;

static size_t str_len(const char *s) {
	size_t n = 0;
	while (s && s[n]) ++n;
	return n;
}

static int str_eq(const char *a, const char *b) {
	if (!a || !b) return 0;
	while (*a && *b) if (*a++ != *b++) return 0;
	return *a == 0 && *b == 0;
}

static int str_has_slash(const char *s) {
	while (s && *s) if (*s++ == '/') return 1;
	return 0;
}

static int str_starts(const char *s, const char *prefix) {
	if (!s || !prefix) return 0;
	while (*prefix) if (*s++ != *prefix++) return 0;
	return 1;
}

static void copy_text(char *dst, size_t cap, const char *src) {
	if (!dst || !cap) return;
	size_t n = 0;
	while (src && src[n] && n + 1u < cap) { dst[n] = src[n]; ++n; }
	dst[n] = 0;
}

static int append_text(char *dst, size_t cap, const char *src) {
	if (!dst || !cap) return 0;
	size_t n = str_len(dst), i = 0;
	while (src && src[i]) {
		if (n + 1u >= cap) return 0;
		dst[n++] = src[i++];
	}
	dst[n] = 0;
	return 1;
}

static int parse_u32(const char *s, uint32_t *out) {
	if (!s || !*s || !out) return 0;
	uint64_t v = 0;
	while (*s) {
		if (*s < '0' || *s > '9') return 0;
		v = v * 10u + (uint32_t)(*s++ - '0');
		if (v > 0xFFFFFFFFu) return 0;
	}
	*out = (uint32_t)v;
	return 1;
}

static void vfs_error(cell_vfs_status_t status, char *out, size_t cap) {
	const char *text = "I/O error.";
	switch (status) {
	case CELL_VFS_NOT_FOUND: text = "No such file or directory."; break;
	case CELL_VFS_NOT_DIR: text = "Not a directory."; break;
	case CELL_VFS_IS_DIR: text = "Is a directory."; break;
	case CELL_VFS_READ_ONLY: text = "Read-only file system."; break;
	case CELL_VFS_EXISTS: text = "File exists."; break;
	case CELL_VFS_FULL: text = "No space left on device."; break;
	case CELL_VFS_INVALID: text = "Invalid argument."; break;
	case CELL_VFS_IO: text = "I/O error."; break;
	case CELL_VFS_NOT_EMPTY: text = "Directory not empty."; break;
	case CELL_VFS_BUSY: text = "Device or resource busy."; break;
	case CELL_VFS_TOO_LARGE: text = "File too large."; break;
	case CELL_VFS_BINARY: text = "Binary file; use stat or execute it directly."; break;
	default: break;
	}
	copy_text(out, cap, text);
}

static int push_word(shell_words_t *w, size_t *used, const char *src, size_t n) {
	if (w->argc >= CELL_SHELL_MAX_ARGS || *used + n + 1u > sizeof(w->storage)) return 0;
	char *dst = &w->storage[*used];
	for (size_t i = 0; i < n; ++i) dst[i] = src[i];
	dst[n] = 0;
	w->argv[w->argc++] = dst;
	*used += n + 1u;
	return 1;
}

/*
 * Deliberately small POSIX-shell lexical subset: words, single/double quotes,
 * backslash quoting, and stdout redirection with > or >>.  Expansion, pipes,
 * command lists, globbing, variables, and job control are future work.
 */
static int parse_words(const char *line, shell_words_t *w) {
	if (!line || !w) return 0;
	for (size_t i = 0; i < sizeof(*w); ++i) ((unsigned char *)w)[i] = 0;
	char token[CELL_SHELL_BUFFER];
	size_t tn = 0, used = 0;
	int token_started = 0;
	int quote = 0;
	int escaped = 0;
	for (size_t i = 0;; ++i) {
		char c = line[i];
		if (escaped) {
			if (!c || tn + 1u >= sizeof(token)) { w->syntax_error = 1; return 1; }
			token[tn++] = c;
			token_started = 1;
			escaped = 0;
			continue;
		}
		if (quote == '\'') {
			if (!c) { w->syntax_error = 1; return 1; }
			if (c == '\'') { quote = 0; continue; }
			if (tn + 1u >= sizeof(token)) { w->syntax_error = 1; return 1; }
			token[tn++] = c;
			token_started = 1;
			continue;
		}
		if (quote == '"') {
			if (!c) { w->syntax_error = 1; return 1; }
			if (c == '"') { quote = 0; continue; }
			if (c == '\\') { escaped = 1; continue; }
			if (tn + 1u >= sizeof(token)) { w->syntax_error = 1; return 1; }
			token[tn++] = c;
			token_started = 1;
			continue;
		}
		if (c == '\\') { escaped = 1; continue; }
		if (c == '\'' || c == '"') { quote = c; token_started = 1; continue; }
		if (c == '|' || c == '&' || c == ';' || c == '<') {
			w->unsupported_operator = 1;
			return 1;
		}
		if (!c || c == ' ' || c == '\t' || c == '>') {
			if (token_started) {
				if (!push_word(w, &used, token, tn)) { w->syntax_error = 1; return 1; }
				tn = 0;
				token_started = 0;
			}
			if (c == '>') {
				if (w->redirect) { w->syntax_error = 1; return 1; }
				w->append = line[i + 1u] == '>';
				if (w->append) ++i;
				while (line[i + 1u] == ' ' || line[i + 1u] == '\t') ++i;
				size_t j = i + 1u, rn = 0;
				char rbuf[CELL_VFS_PATH_MAX];
				int rq = 0, resc = 0;
				for (;; ++j) {
					char rc = line[j];
					if (resc) { if (!rc || rn + 1u >= sizeof(rbuf)) { w->syntax_error=1; return 1; } rbuf[rn++]=rc; resc=0; continue; }
					if (rq == '\'') { if (!rc) { w->syntax_error=1; return 1; } if (rc=='\'') { rq=0; continue; } if (rn+1u>=sizeof(rbuf)){w->syntax_error=1;return 1;} rbuf[rn++]=rc; continue; }
					if (rq == '"') { if (!rc){w->syntax_error=1;return 1;} if(rc=='"'){rq=0;continue;} if(rc=='\\'){resc=1;continue;} if(rn+1u>=sizeof(rbuf)){w->syntax_error=1;return 1;} rbuf[rn++]=rc; continue; }
					if (rc=='\\'){resc=1;continue;} if(rc=='\''||rc=='"'){rq=rc;continue;}
					if (!rc || rc==' ' || rc=='\t') break;
					if (rc=='>' || rc=='|' || rc=='&' || rc==';' || rc=='<') { w->syntax_error=1; return 1; }
					if (rn + 1u >= sizeof(rbuf)) { w->syntax_error=1; return 1; }
					rbuf[rn++] = rc;
				}
				if (!rn || !push_word(w, &used, rbuf, rn)) { w->syntax_error = 1; return 1; }
				w->redirect = w->argv[--w->argc];
				while (line[j] == ' ' || line[j] == '\t') ++j;
				if (line[j]) { w->syntax_error = 1; return 1; }
				return 1;
			}
			if (!c) break;
			continue;
		}
		if (tn + 1u >= sizeof(token)) { w->syntax_error = 1; return 1; }
		token[tn++] = c;
		token_started = 1;
	}
	return 1;
}

static cell_shell_result_t finish_vfs(cell_vfs_status_t status, char *out, size_t cap) {
	if (status != CELL_VFS_OK) vfs_error(status, out, cap);
	return CELL_SHELL_VFS;
}

static int program_candidate(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *command, char path[CELL_VFS_PATH_MAX]) {
	if (!vfs || !command || !*command) return 0;
	if (str_has_slash(command)) copy_text(path, CELL_VFS_PATH_MAX, command);
	else {
		copy_text(path, CELL_VFS_PATH_MAX, "/programs/");
		if (!append_text(path, CELL_VFS_PATH_MAX, command)) return 0;
	}
	char statbuf[256];
	return cell_vfs_stat(vfs, env, path, statbuf, sizeof(statbuf)) == CELL_VFS_OK &&
		str_starts(statbuf, "file ");
}

static int write_redirect(cell_vfs_t *vfs, const char *path, const char *text, int append,
	char *out, size_t cap) {
	cell_vfs_status_t status = cell_vfs_write(vfs, path, text, append);
	if (status != CELL_VFS_OK) { vfs_error(status, out, cap); return 0; }
	out[0] = 0;
	return 1;
}

static int join_echo(const shell_words_t *w, uint32_t start, char *out, size_t cap, int newline) {
	if (!out || !cap) return 0;
	out[0] = 0;
	for (uint32_t i = start; i < w->argc; ++i) {
		if (i != start && !append_text(out, cap, " ")) return 0;
		if (!append_text(out, cap, w->argv[i])) return 0;
	}
	if (newline && !append_text(out, cap, "\n")) return 0;
	return 1;
}


static cell_shell_result_t compile_c(const shell_words_t *w, const cell_capability_env_t *env,
	char *out, size_t cap) {
	if (!env || !env->vfs) { copy_text(out, cap, "cc: file system unavailable."); return CELL_SHELL_VFS; }
	if (w->redirect) { copy_text(out, cap, "cc: shell redirection is not supported for compiler diagnostics."); return CELL_SHELL_VFS; }
	const char *src = 0;
	const char *dst = "a.out";
	for (uint32_t i = 1; i < w->argc; ++i) {
		if (str_eq(w->argv[i], "-o")) {
			if (++i >= w->argc) { copy_text(out, cap, "cc: option requires an argument -- o"); return CELL_SHELL_VFS; }
			dst = w->argv[i];
		} else if (!src) src = w->argv[i];
		else { copy_text(out, cap, "cc: too many input files."); return CELL_SHELL_VFS; }
	}
	if (!src) { copy_text(out, cap, "cc: no input files."); return CELL_SHELL_VFS; }
	size_t source_bytes = 0;
	cell_vfs_status_t vs = cell_vfs_read_bytes(env->vfs, src, cc_source, sizeof(cc_source), &source_bytes);
	if (vs != CELL_VFS_OK) { copy_text(out, cap, "cc: "); append_text(out, cap, src); append_text(out, cap, ": ");
		char err[64]; vfs_error(vs, err, sizeof(err)); append_text(out, cap, err); return CELL_SHELL_VFS; }
	cell_cc_diag_t d; size_t image_bytes = 0;
	if (!cell_cc_compile(cc_source, source_bytes, cc_image, sizeof(cc_image), &image_bytes, &d)) {
		copy_text(out, cap, "cc: "); append_text(out, cap, src); append_text(out, cap, ":");
		char num[16]; uint32_t v=d.line, n=0; char rev[16]; if (!v) v=1; do { rev[n++]=(char)('0'+v%10u); v/=10u; } while(v&&n<sizeof(rev));
		uint32_t j=0; while(n&&j+1u<sizeof(num)) num[j++]=rev[--n]; num[j]=0; append_text(out,cap,num); append_text(out,cap,":");
		v=d.column; n=0; if(!v)v=1; do { rev[n++]=(char)('0'+v%10u); v/=10u; } while(v&&n<sizeof(rev)); j=0; while(n&&j+1u<sizeof(num))num[j++]=rev[--n];num[j]=0;
		append_text(out,cap,num); append_text(out,cap,": error: "); append_text(out,cap,d.message[0]?d.message:cell_cc_status_name(d.status));
		return CELL_SHELL_VFS;
	}
	vs = cell_vfs_write_bytes(env->vfs, dst, cc_image, image_bytes, 0);
	if (vs != CELL_VFS_OK) { copy_text(out, cap, "cc: "); append_text(out, cap, dst); append_text(out, cap, ": "); char err[64]; vfs_error(vs, err, sizeof(err)); append_text(out,cap,err); return CELL_SHELL_VFS; }
	out[0] = 0;
	return CELL_SHELL_VFS;
}

cell_shell_result_t cell_shell_execute(const char *line,
	const cell_capability_env_t *env, char *out, size_t cap, uint32_t *pid) {
	if (!line || !env || !out || !cap) return CELL_SHELL_NOT_HANDLED;
	out[0] = 0;
	if (pid) *pid = 0;
	shell_words_t w;
	if (!parse_words(line, &w)) return CELL_SHELL_NOT_HANDLED;
	if (w.syntax_error) { copy_text(out, cap, "sh: syntax error."); return CELL_SHELL_VFS; }
	if (w.unsupported_operator) { copy_text(out, cap, "sh: operator not implemented yet."); return CELL_SHELL_VFS; }
	if (!w.argc) return CELL_SHELL_NOT_HANDLED;
	const char *cmd = w.argv[0];

	if (str_eq(cmd, "cc")) return compile_c(&w, env, out, cap);

	if (str_eq(cmd, "pwd")) {
		if (w.argc != 1u || w.redirect) { copy_text(out, cap, "pwd: invalid argument."); return CELL_SHELL_VFS; }
		return finish_vfs(env->vfs ? cell_vfs_pwd(env->vfs, out, cap) : CELL_VFS_IO, out, cap);
	}
	if (str_eq(cmd, "cd")) {
		if (w.redirect || w.argc > 2u) { copy_text(out, cap, "cd: invalid argument."); return CELL_SHELL_VFS; }
		const char *path = w.argc == 2u ? w.argv[1] : "/home";
		char absolute[CELL_VFS_PATH_MAX];
		cell_vfs_status_t s = env->vfs ? cell_vfs_chdir(env->vfs, env, path, absolute, sizeof(absolute)) : CELL_VFS_IO;
		if (s == CELL_VFS_OK) out[0] = 0;
		return finish_vfs(s, out, cap);
	}
	if (str_eq(cmd, "ls")) {
		if (w.redirect || w.argc > 2u) { copy_text(out, cap, "ls: options are not implemented yet."); return CELL_SHELL_VFS; }
		return finish_vfs(env->vfs ? cell_vfs_list(env->vfs, env, w.argc == 2u ? w.argv[1] : 0, out, cap) : CELL_VFS_IO, out, cap);
	}
	if (str_eq(cmd, "cat") || str_eq(cmd, "stat")) {
		if (w.argc != 2u || w.redirect) { copy_text(out, cap, str_eq(cmd,"cat") ? "cat: one path is required." : "stat: one path is required."); return CELL_SHELL_VFS; }
		cell_vfs_status_t s = !env->vfs ? CELL_VFS_IO :
			(str_eq(cmd, "cat") ? cell_vfs_cat(env->vfs, env, w.argv[1], out, cap) : cell_vfs_stat(env->vfs, env, w.argv[1], out, cap));
		return finish_vfs(s, out, cap);
	}
	if (str_eq(cmd, "mkdir") || str_eq(cmd, "touch") || str_eq(cmd, "rm") || str_eq(cmd, "rmdir")) {
		if (w.argc < 2u || w.redirect) { copy_text(out, cap, "sh: missing operand."); return CELL_SHELL_VFS; }
		if (!env->vfs) return finish_vfs(CELL_VFS_IO, out, cap);
		for (uint32_t i = 1; i < w.argc; ++i) {
			cell_vfs_status_t s;
			if (str_eq(cmd, "mkdir")) s = cell_vfs_mkdir(env->vfs, w.argv[i]);
			else if (str_eq(cmd, "touch")) s = cell_vfs_touch(env->vfs, w.argv[i]);
			else s = cell_vfs_remove(env->vfs, w.argv[i], str_eq(cmd, "rmdir"));
			if (s != CELL_VFS_OK) return finish_vfs(s, out, cap);
		}
		out[0] = 0;
		return CELL_SHELL_VFS;
	}
	if (str_eq(cmd, "echo")) {
		uint32_t start = 1u;
		int newline = 1;
		if (start < w.argc && str_eq(w.argv[start], "-n")) { newline = 0; ++start; }
		char text[CELL_VFS_TEXT_MAX];
		if (!join_echo(&w, start, text, sizeof(text), w.redirect ? newline : 0)) {
			copy_text(out, cap, "echo: output too large."); return CELL_SHELL_VFS;
		}
		if (w.redirect) {
			if (!env->vfs) return finish_vfs(CELL_VFS_IO, out, cap);
			(void)write_redirect(env->vfs, w.redirect, text, w.append, out, cap);
			return CELL_SHELL_VFS;
		}
		copy_text(out, cap, text);
		return CELL_SHELL_VFS;
	}
	if (str_eq(cmd, "ps")) {
		if (w.redirect) { copy_text(out, cap, "ps: redirection is not implemented yet."); return CELL_SHELL_PROCESS; }
		if (!env->tasks) { copy_text(out, cap, "ps: process table unavailable."); return CELL_SHELL_PROCESS; }
		if (w.argc == 1u) {
			(void)cell_task_list(env->tasks, out, cap);
			return CELL_SHELL_PROCESS;
		}
		if (w.argc == 3u && str_eq(w.argv[1], "-p")) {
			uint32_t id = 0;
			if (!parse_u32(w.argv[2], &id)) copy_text(out, cap, "ps: invalid process id.");
			else (void)cell_task_describe(env->tasks, id, out, cap);
			if (pid) *pid = id;
			return CELL_SHELL_PROCESS;
		}
		copy_text(out, cap, "ps: usage: ps [-p PID]");
		return CELL_SHELL_PROCESS;
	}

	char path[CELL_VFS_PATH_MAX];
	if (!env->vfs || !env->tasks) return CELL_SHELL_NOT_HANDLED;
	if (!program_candidate(env->vfs, env, cmd, path)) {
		if (str_has_slash(cmd)) {
			copy_text(out, cap, "No such file or directory.");
			return CELL_SHELL_PROCESS;
		}
		return CELL_SHELL_NOT_HANDLED;
	}
	char program_output[CELL_TASK_OUTPUT_MAX];
	uint32_t id = 0;
	if (!cell_task_run_argv(env->tasks, env->vfs, env, path, w.argc,
		(const char *const *)w.argv, program_output, sizeof(program_output), &id)) {
		copy_text(out, cap, cmd); append_text(out, cap, ": execution failed.");
		return CELL_SHELL_PROCESS;
	}
	if (pid) *pid = id;
	if (w.redirect) {
		(void)write_redirect(env->vfs, w.redirect, program_output, w.append, out, cap);
		return CELL_SHELL_PROCESS;
	}
	copy_text(out, cap, program_output);
	return CELL_SHELL_PROCESS;
}
