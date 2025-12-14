/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "abi/msg.h"
#include "core/channel.h"
#include "core/parcel.h"

typedef enum {
	ORG_OK = 0,
	ORG_ERR_DENY,
	ORG_ERR_BUDGET,
	ORG_ERR_FORMAT,
	ORG_ERR_ROUTE,
	ORG_ERR_INTERNAL
} org_status_t;

typedef struct {
	msg_t hdr;
	const uint8_t* payload;
} org_call_t;

typedef struct {
	uint8_t buf[1024];
	size_t  len;
} org_reply_t;

static inline bool org_reply_pack(uint32_t ch, uint32_t verb, const void* p, uint32_t n, org_reply_t* out) {
	if (!out)
		return false;
	size_t out_len = 0;
	return parcel_encode(out->buf, sizeof(out->buf), ch, verb, 0u, p, n, NULL, &out_len) && (out->len = out_len, true);
}
