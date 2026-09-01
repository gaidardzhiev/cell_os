/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include "cortex/session.h"
#include "core/shell.h"

static size_t str_len_cap(const char *s, size_t cap) {
	size_t n = 0;
	while (s && n < cap && s[n]) ++n;
	return n;
}

static int starts_call(const char *s) {
	return s && s[0]=='<' && s[1]=='C' && s[2]=='A' && s[3]=='L' && s[4]=='L' && s[5]==' ';
}

static void copy_text(char *dst, size_t cap, const char *src) {
	if (!dst || !cap) return;
	size_t n = 0;
	while (src && src[n] && n + 1u < cap) { dst[n] = src[n]; ++n; }
	dst[n] = 0;
}

static int valid_ascii_text(const char *s, size_t max_len) {
	if (!s) return 0;
	size_t n = 0;
	while (s[n]) {
		if (n >= max_len || (unsigned char)s[n] < 32u || (unsigned char)s[n] > 126u) return 0;
		++n;
	}
	return n != 0u;
}

static int feed_text(cortex_t *c, const char *s) {
	while (s && *s) {
		if (c->pos >= c->model.h->context_len - 1u || !cortex_feed(c, (uint8_t)*s++)) return 0;
	}
	return 1;
}

static int generate_line(cortex_t *c, char *dst, size_t cap) {
	if (!c || !dst || cap < 2u) return 0;
	size_t n = 0;
	dst[0] = 0;
	while (n + 1u < cap && c->pos < c->model.h->context_len - 1u) {
		uint8_t t = cortex_next(c);
		if (!t || t == '\r' || t == '\n') break;
		if (t < 32u || t > 126u) return 0;
		dst[n++] = (char)t;
		dst[n] = 0;
		if (!cortex_feed(c, t)) return 0;
	}
	return n != 0u;
}

int cortex_session_process(cortex_t *c, const char *user_text,
	const cell_capability_env_t *env, cortex_session_result_t *out) {
	if (!c || !user_text || !env || !out) return 0;
	out->kind = CORTEX_SESSION_ERROR;
	out->capability = CELL_CAP_INVALID;
	out->first_pass[0] = 0;
	out->response[0] = 0;
	out->result.text[0] = 0;
	out->task_id = 0;

	if (!valid_ascii_text(user_text, CORTEX_SESSION_INPUT_MAX)) return 0;
	cell_shell_result_t shell = cell_shell_execute(user_text, env, out->response,
		sizeof(out->response), &out->task_id);
	if (shell != CELL_SHELL_NOT_HANDLED) {
		out->kind = shell == CELL_SHELL_PROCESS ? CORTEX_SESSION_TASK : CORTEX_SESSION_VFS;
		return 1;
	}
	if (!valid_ascii_text(user_text, 95u)) return 0;

	/* CellLM remains an intent router for natural-language requests. */
	cortex_reset(c);
	if (!feed_text(c, "cell> ") || !feed_text(c, user_text) || !feed_text(c, "\n")) return 0;
	if (!generate_line(c, out->first_pass, sizeof(out->first_pass))) return 0;

	cell_capability_id_t id;
	size_t first_len = str_len_cap(out->first_pass, sizeof(out->first_pass));
	if (!cell_capability_parse_call(out->first_pass, first_len, &id)) {
		if (starts_call(out->first_pass)) {
			copy_text(out->response, sizeof(out->response),
				"Cell Cortex refused an unknown capability request.");
			out->kind = CORTEX_SESSION_DIRECT;
			return 1;
		}
		out->kind = CORTEX_SESSION_DIRECT;
		copy_text(out->response, sizeof(out->response), out->first_pass);
		return 1;
	}

	out->capability = id;
	if (!cell_capability_execute(env, id, &out->result)) return 0;
	if (!cell_capability_human_response(&out->result, out->response, sizeof(out->response))) return 0;
	out->kind = CORTEX_SESSION_CAPABILITY;
	return 1;
}
