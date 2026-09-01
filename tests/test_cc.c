/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "core/cc.h"
#include "core/capability.h"
#include "core/cellexec.h"

static int expect(int cond, const char *what, const cell_cc_diag_t *d) {
	if (cond) return 1;
	fprintf(stderr, "FAIL: %s", what);
	if (d) fprintf(stderr, " (%u:%u %s)", d->line, d->column, d->message);
	fputc('\n', stderr);
	return 0;
}

int main(void) {
	static uint8_t image[CELL_EXEC_FILE_MAX]; size_t bytes = 0; cell_cc_diag_t d; int ok = 1;
	const char *loop =
		"#include <stdio.h>\n"
		"int main(void) { int n = 3; while (n) { puts(\"tick\"); n = n - 1; } return 0; }\n";
	ok &= expect(cell_cc_compile(loop, strlen(loop), image, sizeof(image), &bytes, &d), "compile loop C subset", &d);
	cell_exec_t e;
	ok &= expect(cell_exec_open(&e, image, bytes, ~0ull) == CELL_EXEC_OK, "CellExec validation", &d);
	ok &= expect(e.instruction_count >= 7u && e.h->capability_mask == 0u, "loop code shape", &d);

	const char *cap =
		"#include <cell.h>\nint main(void) { cell_capability(\"memory.status\"); return 0; }\n";
	ok &= expect(cell_cc_compile(cap, strlen(cap), image, sizeof(image), &bytes, &d), "compile capability call", &d);
	ok &= expect(cell_exec_open(&e, image, bytes, ~0ull) == CELL_EXEC_OK &&
		(e.h->capability_mask & (1ull << CELL_CAP_MEMORY_STATUS)), "capability declaration inferred", &d);

	const char *argv_source =
		"#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n"
		"int main(int argc, char **argv) { char *p; int n; p = argv[1]; "
		"n = atoi(argv[2]); if (strlen(p) >= 2) n = n / 2 + n % 2; "
		"if (strcmp(p, \"cell\") == 0) putchar('C'); return n; }\n";
	ok &= expect(cell_cc_compile(argv_source, strlen(argv_source), image, sizeof(image), &bytes, &d),
		"compile argc argv and libc subset", &d);
	ok &= expect(cell_exec_open(&e, image, bytes, ~0ull) == CELL_EXEC_OK &&
		e.h->memory_bytes == CELL_CC_TASK_MEMORY, "argv program CellExec validation", &d);

	const char *bad = "int main(void) { printf(\"x\"); return 0; }";
	ok &= expect(!cell_cc_compile(bad, strlen(bad), image, sizeof(image), &bytes, &d) &&
		d.status == CELL_CC_PARSE_ERROR, "reject unsupported C library surface", &d);
	if (!ok) return 1;
	puts("#CC C subset parse/codegen PASS");
	puts("#CC CellExec validation PASS");
	puts("#CC capability inference PASS");
	puts("#CC unsupported-surface rejection PASS");
	puts("#CC argc argv pointer libc subset PASS");
	return 0;
}
