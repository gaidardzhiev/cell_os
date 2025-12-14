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

int main(void){
	irq_bridge_t B;
	irq_policy_t P = { .debounce_ticks=0, .batch_max=8, .rl_refill=8, .rl_bucket_max=8 };
	irq_init(&B, &P);
	uint32_t sA = irq_register_source(&B, 10);
	uint32_t sB = irq_register_source(&B, 11);
	(void)irq_raise(&B, sA, EV_TIMER, "A", 1);
	(void)irq_raise(&B, sB, EV_TIMER, "B", 1);
	(void)irq_raise(&B, sA, EV_TIMER, "C", 1);
	assert(B.seq == 3);
	event_frame_t e1,e2,e3;
	assert(irq_next(&B,&e1));
	assert(irq_next(&B,&e2));
	assert(irq_next(&B,&e3));
	assert(memcmp(e1.buf+16,"A",1)==0);
	assert(memcmp(e2.buf+16,"B",1)==0);
	assert(memcmp(e3.buf+16,"C",1)==0);
	puts("#E6 order ok");
	return 0;
}
