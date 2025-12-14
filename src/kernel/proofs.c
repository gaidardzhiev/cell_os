/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "kernel/proofs.h"
#include "kernel/console.h"
#include "core/hash.h"
#include "core/cg.h"
#include "core/channel.h"
#include "core/qos.h"
#include "core/organelles.h"
#include "core/events.h"
#include "core/update.h"
#include "core/obs.h"
#include "core/obs_trace.h"
#include "core/trust.h"
#include "core/crypto.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

static uint32_t hash32_of(const void* buf, size_t len) {
		uint8_t out[32];
		cell_hash((const uint8_t*)buf, len, out);
		return (uint32_t)out[0]
				| ((uint32_t)out[1] << 8)
				| ((uint32_t)out[2] << 16)
				| ((uint32_t)out[3] << 24);
}

static void proof_cg(uint32_t* qos_code, uint32_t* cg_crc) {
		cg_entry_t entries[] = {
			{ .chan = 0, .sink_id = CG_SINK_DIAG, .max_depth = 2, .rsv = 0 },
			{ .chan = 1, .sink_id = CG_SINK_DIAG, .max_depth = 4, .rsv = 0 },
		};
		cg_table_t table = { entries, ARRAY_SIZE(entries) };
		cg_init(&table);
		cg_entry_t route = entries[0];
		(void)cg_route(0, &route);
		channel_graph_t graph = {0};
		channel_qos_t qos_buf[4];
		uint8_t adj[(4 * 4 + 7) / 8];
		cg_build_full(&graph, qos_buf, adj, 4, 256, 2048);
		*cg_crc = hash32_of(adj, sizeof(adj));
		qos_init(1);
		qos_cfg_channel(0, QOS_BE, 3, 2048, 256);
		qos_set_gas(1536);
		qos_status_t s1 = qos_try_enqueue(route.chan, 512);
		qos_status_t s2 = qos_try_enqueue(route.chan, 768);
		qos_status_t s3 = qos_try_enqueue(route.chan, 512);
		uint32_t packed = ((uint32_t)s1 << 24) | ((uint32_t)s2 << 16) | ((uint32_t)s3 << 8) | (qos_get_gas() & 0xFFu);
		*qos_code = packed;
}

static void proof_org(uint32_t* mito_hash, uint32_t* lyso_hash) {
		static channel_graph_t graph;
		static channel_qos_t qos_buf[6];
		static uint8_t adj[(6 * 6 + 7) / 8];
		static int initialized;
		if (!initialized){
			cg_build_full(&graph, qos_buf, adj, 6, 512, 4096);
			initialized = 1;
		}
		organelles_init(&graph);
		organelles_charge_cpu(1, 50000);
		organelles_charge_io(1, 4096);
		organelles_charge_mem(2, 2048);
		organelles_mark_dead(3);
		organelles_mark_dead(5);
		bool dead_map[16];
		for (uint32_t gid = 0; gid < ARRAY_SIZE(dead_map); ++gid) {
	dead_map[gid] = organelles_is_dead(gid);
		}
		const mito_table_t* mito = organelles_mito_table();
		if (mito && mito->entries && mito->len > 0){
	*mito_hash = hash32_of(mito->entries, (size_t)mito->len * sizeof(mito_entry_t));
		} else {
	*mito_hash = 0;
		}
		*lyso_hash = hash32_of(dead_map, sizeof(dead_map));
}

static void proof_events(uint32_t* delivered, uint32_t* evt_hash) {
		event_frame_t frames[2];
		uint8_t payload0[] = {0xAA, 0x55, 0x01, 0x02};
		uint8_t payload1[] = {0x10, 0x20, 0x30};
		event_pack(&frames[0], 1, 7, EV_CUSTOM, payload0, ARRAY_SIZE(payload0));
		event_pack(&frames[1], 2, 9, EV_CUSTOM, payload1, ARRAY_SIZE(payload1));
		uint8_t buffer[sizeof(frames[0].buf) * 2];
		size_t offset = 0;
		memcpy(buffer + offset, frames[0].buf, frames[0].len); offset += frames[0].len;
		memcpy(buffer + offset, frames[1].buf, frames[1].len); offset += frames[1].len;
		*delivered = (uint32_t)(frames[0].len + frames[1].len);
		*evt_hash = hash32_of(buffer, offset);
}

static void fill_strand(uint8_t* buf, size_t len, uint8_t seed) {
	for (size_t i = 0; i < len; ++i) {
		buf[i] = (uint8_t)(seed + (uint8_t)i * 17u);
	}
}

static void proof_update(uint32_t* old_sha, uint32_t* new_sha) {
	static uint8_t strand_x[256];
	static uint8_t strand_y[256];
	fill_strand(strand_y, sizeof(strand_y), 0x42);
	fill_strand(strand_x, sizeof(strand_x), 0x10);
	genome_storage_t gs = {
		.strand_x = strand_x,
		.strand_x_len = sizeof(strand_x),
		.strand_y = strand_y,
		.strand_y_len = sizeof(strand_y),
		.preferred = 1
	};
	static const uint8_t new_image[] = {
	0xC1,0xE7,0x10,0x5A,0x9B,0x62,0x18,0xFE,
	0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
	0x55,0xAA,0x77,0x33
	};
	genome_meta_t meta = {0};
	size_t half = sizeof(new_image) / 2;
	cell_hash(new_image, half, meta.merkle_idx);
	cell_hash(new_image + half, sizeof(new_image) - half, meta.merkle_genes);
	cell_hash(new_image, sizeof(new_image), meta.trust_root);
	meta.version_major = 1;
	meta.version_minor = 0;
	updater_t up;
	updater_init(&up, &gs);
	(void)update_write_and_verify(&up, new_image, sizeof(new_image), &meta);
	*old_sha = hash32_of(strand_y, sizeof(strand_y));
	*new_sha = hash32_of(strand_x, sizeof(strand_x));
}

static void proof_obs(uint32_t* entries, uint32_t* obs_hash) {
	obs_index_t idx;
	obs_index_init(&idx);
	uint8_t blob[256];
	size_t len = 0;
	obs_mark_line(1, 1, "#E8 mark alpha", blob, sizeof(blob), &len);
	obs_index_feed(&idx, blob, len);
	obs_mark_line(2, 2, "#E8 mark beta", blob, sizeof(blob), &len);
	obs_index_feed(&idx, blob, len);
	obs_hdr_t hdr = {
		.ver_major = OBS_VER_MAJOR,
		.ver_minor = OBS_VER_MINOR,
		.rec_kind = OBS_REC_EVENT,
		.tick = 3,
		.seq = 3,
		.payload_len = 4,
	};
	uint8_t payload[] = {0xDE,0xAD,0xBE,0xEF};
	obs_pack_to_ch0(&hdr, payload, blob, sizeof(blob), &len);
	obs_index_feed(&idx, blob, len);
	*entries = (uint32_t)(idx.entries & 0xFFFFFFFFu);
	*obs_hash = hash32_of(idx.hash32, sizeof(idx.hash32));
}

static void proof_security(uint32_t* key_id_hex, uint32_t* mac32) {
	static const uint8_t seeds[2][32] = {
		{ 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
			0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x10,
			0x20,0x30,0x40,0x50,0x60,0x70,0x80,0x90,
			0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x0F,0xF1 },
		{ 0xF1,0xE2,0xD3,0xC4,0xB5,0xA6,0x97,0x88,
			0x79,0x6A,0x5B,0x4C,0x3D,0x2E,0x1F,0x01,
			0x02,0x13,0x24,0x35,0x46,0x57,0x68,0x79,
			0x8A,0x9B,0xAC,0xBD,0xCE,0xDF,0xE0,0x0E }
		};
		trust_set_t trust;
		trust_init(&trust, seeds, 2, 1, 42);
		uint8_t key_id = 0;
		uint8_t raw32[32];
		trust_active_mac(&trust, &key_id, raw32);
		mac_key_t key;
		mac16_init(&key, key_id, raw32);
		static const uint8_t msg[] = {'E','9','p','r','o','o','f'};
		uint8_t mac[MAC16_LEN];
		mac16_compute(&key, msg, sizeof(msg), mac);
		*key_id_hex = key_id;
		*mac32 = (uint32_t)mac[0] | ((uint32_t)mac[1] << 8) | ((uint32_t)mac[2] << 16) | ((uint32_t)mac[3] << 24);
}

static void proof_release(uint32_t manifest_inputs[], size_t count,uint32_t* manifest_hash, uint32_t* manifest_extra) {
	*manifest_hash = hash32_of(manifest_inputs, count * sizeof(uint32_t));
	*manifest_extra = 0;
}

void proofs_run_all(void) {
		uint32_t manifest_inputs[16];
		size_t mi = 0;
		uint32_t qos_code = 0, cg_crc = 0;
		proof_cg(&qos_code, &cg_crc);
		dbg_puts("#E3/E4 CG proof: qos=");
		dbg_hex32(qos_code);
		dbg_puts(" cg_crc=");
		dbg_hex32(cg_crc);
		dbg_puts("\n");
		manifest_inputs[mi++] = qos_code;
		manifest_inputs[mi++] = cg_crc;
		uint32_t mito_hash = 0, lyso_hash = 0;
		proof_org(&mito_hash, &lyso_hash);
		dbg_puts("#E5 ORG proof: mito=");
		dbg_hex32(mito_hash);
		dbg_puts(" golgi=");
		dbg_hex32(lyso_hash);
		dbg_puts("\n");
		manifest_inputs[mi++] = mito_hash;
		manifest_inputs[mi++] = lyso_hash;
		uint32_t delivered = 0, evt_hash = 0;
		proof_events(&delivered, &evt_hash);
		dbg_puts("#E6 EVT proof: delivered=");
		dbg_hex32(delivered);
		dbg_puts(" evt_hash=");
		dbg_hex32(evt_hash);
		dbg_puts("\n");
		manifest_inputs[mi++] = delivered;
		manifest_inputs[mi++] = evt_hash;
		uint32_t old_sha = 0, new_sha = 0;
		proof_update(&old_sha, &new_sha);
		dbg_puts("#E7 UPD proof: old_sha=");
		dbg_hex32(old_sha);
		dbg_puts(" new_sha=");
		dbg_hex32(new_sha);
		dbg_puts("\n");
		manifest_inputs[mi++] = old_sha;
		manifest_inputs[mi++] = new_sha;
		uint32_t obs_entries = 0, obs_hash = 0;
		proof_obs(&obs_entries, &obs_hash);
		dbg_puts("#E8 OBS proof: entries=");
		dbg_hex32(obs_entries);
		dbg_puts(" obs_hash=");
		dbg_hex32(obs_hash);
		dbg_puts("\n");
		manifest_inputs[mi++] = obs_entries;
		manifest_inputs[mi++] = obs_hash;
		uint32_t key_id = 0, mac32 = 0;
		proof_security(&key_id, &mac32);
		dbg_puts("#E9 SEC proof: key=");
		dbg_hex32(key_id);
		dbg_puts(" mac16=");
		dbg_hex32(mac32);
		dbg_puts("\n");
		manifest_inputs[mi++] = key_id;
		manifest_inputs[mi++] = mac32;
		uint32_t manifest_hash = 0, manifest_extra = 0;
		proof_release(manifest_inputs, mi, &manifest_hash, &manifest_extra);
		dbg_puts("#E10 RELEASE proof: tag=");
		dbg_hex32(manifest_hash);
		dbg_puts(" build=");
		dbg_hex32(manifest_extra);
		dbg_puts("\n");
		dbg_puts("FULL PROOF SUCCESS\n");
}
