/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/irq_bridge.h"
#include "core/events.h"

int main(void) {
	irq_bridge_t B;
	irq_policy_t P = { .debounce_ticks=1, .batch_max=8, .rl_refill=2, .rl_bucket_max=4 };
	irq_init(&B, &P);
	uint32_t s0 = irq_register_source(&B, 33);
	for (int i=0;i<10;i++) (void)irq_raise(&B, s0, EV_SERIAL_RX, "x", 1);
	assert(B.q_len <= 1);
	irq_poll(&B);
	for (int i=0;i<10;i++) (void)irq_raise(&B, s0, EV_SERIAL_RX, "y", 1);
	int delivered=0;
	event_frame_t ev;
	for (int i=0;i<16;i++){
		if (!irq_next(&B,&ev)) break;
		delivered++;
	}
	assert(delivered <= (int)P.batch_max);
	puts("#E6 flood ok");
	return 0;
}
