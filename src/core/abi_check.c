/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "abi/nci.h"
#include "abi/cap.h"
#include "abi/msg.h"
#include "core/channel.h"
#include "core/phenotype.h"

#define CAP_TOKEN_EXPECTED_BYTES (36u + 16u)
#define MSG_EXPECTED_BYTES 16u
#define HANDOFF_MAX_BYTES (4u * 1024u)
#define CHANNEL_QOS_EXPECTED_BYTES 36u
#define PHENO_HDR_EXPECTED_BYTES 20u

_Static_assert(sizeof(cap_token_t) == CAP_TOKEN_EXPECTED_BYTES, "cap_token_t layout drifted");
_Static_assert(sizeof(msg_t) == MSG_EXPECTED_BYTES, "msg_t layout drifted");
_Static_assert(sizeof(handoff_t) <= HANDOFF_MAX_BYTES, "handoff_t exceeds ABI contract");
_Static_assert(sizeof(channel_qos_t) == CHANNEL_QOS_EXPECTED_BYTES, "channel_qos_t layout drifted");
_Static_assert(sizeof(phenotype_bin_hdr_t) == PHENO_HDR_EXPECTED_BYTES, "phenotype_bin_hdr_t layout drifted");

static int abi_runtime_checks(void) {
	const size_t cap_sz = sizeof(cap_token_t);
	if (cap_sz != CAP_TOKEN_EXPECTED_BYTES) {
		fprintf(stderr, "ABI ERROR: cap_token_t size = %zu bytes (expected %u)\n", cap_sz, CAP_TOKEN_EXPECTED_BYTES);
		return 1;
	}
	const size_t msg_sz = sizeof(msg_t);
	if (msg_sz != MSG_EXPECTED_BYTES) {
		fprintf(stderr, "ABI ERROR: msg_t size = %zu bytes (expected %u)\n", msg_sz, MSG_EXPECTED_BYTES);
		return 1;
	}
	const size_t handoff_sz = sizeof(handoff_t);
	if (handoff_sz > HANDOFF_MAX_BYTES) {
		fprintf(stderr, "ABI ERROR: handoff_t size = %zu bytes (limit %u)\n", handoff_sz, HANDOFF_MAX_BYTES);
		return 1;
	}
	const size_t qos_sz = sizeof(channel_qos_t);
	if (qos_sz != CHANNEL_QOS_EXPECTED_BYTES) {
		fprintf(stderr, "ABI ERROR: channel_qos_t size = %zu bytes (expected %u)\n", qos_sz, CHANNEL_QOS_EXPECTED_BYTES);
		return 1;
	}
	const size_t hdr_sz = sizeof(phenotype_bin_hdr_t);
	if (hdr_sz != PHENO_HDR_EXPECTED_BYTES) {
		fprintf(stderr, "ABI ERROR: phenotype_bin_hdr_t size = %zu bytes (expected %u)\n", hdr_sz, PHENO_HDR_EXPECTED_BYTES);
		return 1;
	}
	return 0;
}

int main(void) {
	if (abi_runtime_checks() != 0) {
		return 1;
	}
	printf("ABI: cap-token size = %zu bytes (36+16) ... OK\n", sizeof(cap_token_t));
	printf("ABI: channel_qos_t size = %zu bytes ... OK\n", sizeof(channel_qos_t));
	printf("ABI: phenotype_hdr size = %zu bytes ... OK\n", sizeof(phenotype_bin_hdr_t));
	printf("#E1 NCI ok (nci=%u.%u cap=%u.%u msg=%u.%u)\n", NCI_ABI_MAJOR, NCI_ABI_MINOR, CAP_ABI_MAJOR, CAP_ABI_MINOR, MSG_ABI_MAJOR, MSG_ABI_MINOR);
	return 0;
}
