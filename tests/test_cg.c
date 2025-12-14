/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include "core/cg.h"

int main(void) {
		const cg_entry_t entries[] = {
			{ .chan = 0, .sink_id = CG_SINK_DIAG, .max_depth = 2, .rsv = 0 },
			{ .chan = 5, .sink_id = 99, .max_depth = 4, .rsv = 0 }
		};
		const cg_table_t table = { entries, 2 };
		assert(cg_init(&table));
		cg_entry_t out;
		assert(cg_route(0, &out));
		assert(out.sink_id == CG_SINK_DIAG);
		assert(out.max_depth == 2);
		assert(cg_route(5, &out));
		assert(out.sink_id == 99);
		assert(!cg_route(3, &out));
		puts("#E3 test cg ok");
		return 0;
}
