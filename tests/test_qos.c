/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <assert.h>
#include <stdio.h>
#include "core/qos.h"

int main(void) {
	qos_init(1);
	qos_cfg_channel(0, QOS_BE, 1, 1024, 512);
	qos_set_gas(1024);
	assert(qos_try_enqueue(0, 256) == QOS_OK);
	assert(qos_try_enqueue(0, 256) == QOS_BUSY);
	qos_tick();
	qos_set_gas(4096);
	assert(qos_try_enqueue(0, 2048) == QOS_DROP);
	qos_set_gas(0);
	assert(qos_try_enqueue(0, 1) == QOS_EGAS);
	puts("#E3 test qos ok");
	return 0;
}
