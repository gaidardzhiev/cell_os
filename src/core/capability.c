/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include "core/capability.h"
#include "core/vfs.h"

#define MIB 1048576ull

typedef struct {
	char *p;
	char *end;
	int ok;
} sb_t;

static void sb_init(sb_t *b, char *dst, size_t n) {
	b->p = dst;
	b->end = dst + (n ? n - 1u : 0u);
	b->ok = n != 0u;
	if (n) dst[0] = 0;
}

static void sb_char(sb_t *b, char c) {
	if (!b->ok || b->p >= b->end) { b->ok = 0; return; }
	*b->p++ = c;
	*b->p = 0;
}

static void sb_str(sb_t *b, const char *s) {
	while (s && *s) sb_char(b, *s++);
}

static void sb_u64(sb_t *b, uint64_t v) {
	char tmp[24];
	unsigned n = 0;
	if (!v) { sb_char(b, '0'); return; }
	while (v && n < sizeof(tmp)) { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; }
	while (n) sb_char(b, tmp[--n]);
}

static size_t cstr_len(const char *s) {
	size_t n = 0;
	while (s && s[n]) ++n;
	return n;
}

static int bytes_eq(const char *a, size_t an, const char *b) {
	size_t bn = cstr_len(b);
	if (an != bn) return 0;
	for (size_t i = 0; i < an; ++i) if (a[i] != b[i]) return 0;
	return 1;
}

static void small_copy(char *dst, size_t cap, const char *src) {
	if (!dst || !cap) return;
	size_t n = 0;
	while (src && src[n] && n + 1u < cap) { dst[n] = src[n]; ++n; }
	dst[n] = 0;
}

static void result_clear(cell_capability_result_t *r) {
	r->id = CELL_CAP_INVALID;
	r->ok = 0;
	r->text[0] = 0;
	r->top_mib = r->usable_mib = r->free_mib = r->memory_mib = 0;
	r->family = r->model = r->logical = r->count = 0;
	r->vendor[0] = r->cortex_state[0] = r->storage[0] = r->primary[0] = 0;
	r->vfs_text[0] = 0;
}

const char *cell_capability_name(cell_capability_id_t id) {
	switch (id) {
	case CELL_CAP_SYSTEM_STATUS: return "system.status";
	case CELL_CAP_CPU_INFO: return "cpu.info";
	case CELL_CAP_MEMORY_STATUS: return "memory.status";
	case CELL_CAP_STORAGE_LIST: return "storage.list";
	case CELL_CAP_STORAGE_LIST_DIR: return "storage.list_dir";
	case CELL_CAP_STORAGE_PWD: return "storage.pwd";
	case CELL_CAP_NETWORK_LIST: return "network.list";
	case CELL_CAP_GPU_INFO: return "gpu.info";
	case CELL_CAP_USB_LIST: return "usb.list";
	case CELL_CAP_DISPLAY_INFO: return "display.info";
	case CELL_CAP_POWER_STATUS: return "power.status";
	default: return 0;
	}
}

int cell_capability_parse_call(const char *text, size_t len, cell_capability_id_t *id) {
	static const struct { const char *name; cell_capability_id_t id; } table[] = {
		{"system.status", CELL_CAP_SYSTEM_STATUS},
		{"cpu.info", CELL_CAP_CPU_INFO},
		{"memory.status", CELL_CAP_MEMORY_STATUS},
		{"storage.list", CELL_CAP_STORAGE_LIST},
		{"storage.list_dir", CELL_CAP_STORAGE_LIST_DIR},
		{"storage.pwd", CELL_CAP_STORAGE_PWD},
		{"network.list", CELL_CAP_NETWORK_LIST},
		{"gpu.info", CELL_CAP_GPU_INFO},
		{"usb.list", CELL_CAP_USB_LIST},
		{"display.info", CELL_CAP_DISPLAY_INFO},
		{"power.status", CELL_CAP_POWER_STATUS}
	};
	if (!text || !id || len < 8u) return 0;
	while (len && (text[len - 1u] == '\r' || text[len - 1u] == '\n' || text[len - 1u] == ' ')) --len;
	if (len < 8u || text[0] != '<' || text[1] != 'C' || text[2] != 'A' || text[3] != 'L' ||
	    text[4] != 'L' || text[5] != ' ' || text[len - 1u] != '>') return 0;
	const char *name = text + 6u;
	size_t name_len = len - 7u;
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
		if (bytes_eq(name, name_len, table[i].name)) { *id = table[i].id; return 1; }
	}
	return 0;
}

static uint64_t usable_memory_bytes(const cell_capability_env_t *env) {
	if (!env || !env->boot_ext) return 0;
	const cell_boot_ext_t *ext = env->boot_ext;
	if (!ext->e820_ptr || ext->e820_entry_size != sizeof(cell_e820_entry_t)) return 0;
	const cell_e820_entry_t *map = (const cell_e820_entry_t *)(uintptr_t)ext->e820_ptr;
	uint64_t total = 0;
	for (uint32_t i = 0; i < ext->e820_count; ++i) {
		if (map[i].type != 1u) continue;
		if (UINT64_MAX - total < map[i].length) return UINT64_MAX;
		total += map[i].length;
	}
	return total;
}

#if defined(__i386__) || defined(__x86_64__)
static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
	__asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(subleaf));
}

static void cpu_fields(char vendor[13], uint32_t *family, uint32_t *model, uint32_t *logical) {
	uint32_t a, b, c, d;
	cpuid(0, 0, &a, &b, &c, &d);
	vendor[0]=(char)b; vendor[1]=(char)(b>>8); vendor[2]=(char)(b>>16); vendor[3]=(char)(b>>24);
	vendor[4]=(char)d; vendor[5]=(char)(d>>8); vendor[6]=(char)(d>>16); vendor[7]=(char)(d>>24);
	vendor[8]=(char)c; vendor[9]=(char)(c>>8); vendor[10]=(char)(c>>16); vendor[11]=(char)(c>>24); vendor[12]=0;
	cpuid(1, 0, &a, &b, &c, &d);
	uint32_t base_family=(a>>8)&15u, ext_family=(a>>20)&255u;
	uint32_t base_model=(a>>4)&15u, ext_model=(a>>16)&15u;
	*family = base_family == 15u ? base_family + ext_family : base_family;
	*model = (base_family == 6u || base_family == 15u) ? base_model + (ext_model << 4) : base_model;
	*logical = (b >> 16) & 255u;
	if (!*logical) *logical = 1u;
}
#else
static void cpu_fields(char vendor[13], uint32_t *family, uint32_t *model, uint32_t *logical) {
	const char *s = "unknown";
	unsigned i = 0;
	while (s[i] && i < 12) { vendor[i] = s[i]; ++i; }
	vendor[i] = 0;
	*family = 0; *model = 0; *logical = 1;
}
#endif

static int unsupported(cell_capability_result_t *r, const char *reason) {
	sb_t b;
	sb_init(&b, r->text, sizeof(r->text));
	sb_str(&b, "<RESULT "); sb_str(&b, cell_capability_name(r->id));
	sb_str(&b, " status=unsupported reason="); sb_str(&b, reason); sb_char(&b, '>');
	r->ok = 0;
	return b.ok;
}

int cell_capability_execute(const cell_capability_env_t *env, cell_capability_id_t id,
	cell_capability_result_t *r) {
	if (!env || !r || id == CELL_CAP_INVALID) return 0;
	result_clear(r);
	r->id = id;
	sb_t b;
	sb_init(&b, r->text, sizeof(r->text));
	uint64_t usable = usable_memory_bytes(env);
	uint64_t free_bytes = (env->arena && env->arena->end >= env->arena->next) ?
		(uint64_t)(env->arena->end - env->arena->next) : 0;
	uint64_t top = env->handoff ? env->handoff->mem_top : 0;

	switch (id) {
	case CELL_CAP_MEMORY_STATUS:
		r->top_mib = top / MIB;
		r->usable_mib = usable / MIB;
		r->free_mib = free_bytes / MIB;
		sb_str(&b, "<RESULT memory.status status=ok top_mib="); sb_u64(&b, r->top_mib);
		sb_str(&b, " usable_mib="); sb_u64(&b, r->usable_mib);
		sb_str(&b, " free_mib="); sb_u64(&b, r->free_mib); sb_char(&b, '>');
		r->ok = 1;
		break;
	case CELL_CAP_CPU_INFO:
		cpu_fields(r->vendor, &r->family, &r->model, &r->logical);
		sb_str(&b, "<RESULT cpu.info status=ok vendor="); sb_str(&b, r->vendor);
		sb_str(&b, " family="); sb_u64(&b, r->family);
		sb_str(&b, " model="); sb_u64(&b, r->model);
		sb_str(&b, " logical="); sb_u64(&b, r->logical); sb_char(&b, '>');
		r->ok = 1;
		break;
	case CELL_CAP_SYSTEM_STATUS:
		r->memory_mib = usable / MIB;
		small_copy(r->cortex_state, sizeof(r->cortex_state), env->cortex_ready ? "ready" : "not_ready");
		small_copy(r->storage, sizeof(r->storage), env->ata0_ready ? "ata0" : "none");
		sb_str(&b, "<RESULT system.status status=ok cortex="); sb_str(&b, r->cortex_state);
		sb_str(&b, " memory_mib="); sb_u64(&b, r->memory_mib);
		sb_str(&b, " storage="); sb_str(&b, r->storage); sb_char(&b, '>');
		r->ok = 1;
		break;
	case CELL_CAP_STORAGE_LIST:
		if (!env->ata0_ready) return unsupported(r, "no_storage_backend");
		r->count = 1;
		small_copy(r->primary, sizeof(r->primary), "ata0");
		sb_str(&b, "<RESULT storage.list status=ok count=1 primary=ata0>");
		r->ok = 1;
		break;
	case CELL_CAP_STORAGE_LIST_DIR: {
		if (!env->vfs || !env->vfs->mounted) return unsupported(r, "no_vfs");
		cell_vfs_status_t status = cell_vfs_list(env->vfs, env, 0, r->vfs_text, sizeof(r->vfs_text));
		if (status != CELL_VFS_OK) return unsupported(r, cell_vfs_status_name(status));
		sb_str(&b, "<RESULT storage.list_dir status=ok>");
		r->ok = 1;
		break;
	}
	case CELL_CAP_STORAGE_PWD: {
		if (!env->vfs || !env->vfs->mounted) return unsupported(r, "no_vfs");
		cell_vfs_status_t status = cell_vfs_pwd(env->vfs, r->vfs_text, sizeof(r->vfs_text));
		if (status != CELL_VFS_OK) return unsupported(r, cell_vfs_status_name(status));
		sb_str(&b, "<RESULT storage.pwd status=ok>");
		r->ok = 1;
		break;
	}
	case CELL_CAP_NETWORK_LIST: return unsupported(r, "no_network_backend");
	case CELL_CAP_GPU_INFO: return unsupported(r, "no_gpu_backend");
	case CELL_CAP_USB_LIST: return unsupported(r, "no_usb_backend");
	case CELL_CAP_DISPLAY_INFO: return unsupported(r, "no_display_backend");
	case CELL_CAP_POWER_STATUS: return unsupported(r, "no_power_backend");
	default: return 0;
	}
	return b.ok;
}

int cell_capability_human_response(const cell_capability_result_t *r, char *out, size_t out_bytes) {
	if (!r || !out || !out_bytes) return 0;
	sb_t b;
	sb_init(&b, out, out_bytes);
	if (!r->ok) {
		switch (r->id) {
		case CELL_CAP_STORAGE_LIST_DIR:
			sb_str(&b, "Directory listing is unavailable because Cell VFS is not mounted."); break;
		case CELL_CAP_STORAGE_PWD:
			sb_str(&b, "A working directory is unavailable because Cell VFS is not mounted."); break;
		case CELL_CAP_NETWORK_LIST:
			sb_str(&b, "Network information is unavailable because no verified network backend is active."); break;
		case CELL_CAP_GPU_INFO:
			sb_str(&b, "GPU information is unavailable because no verified GPU backend is active."); break;
		case CELL_CAP_USB_LIST:
			sb_str(&b, "USB information is unavailable because no verified USB backend is active."); break;
		case CELL_CAP_DISPLAY_INFO:
			sb_str(&b, "Display information is unavailable because no verified display backend is active."); break;
		case CELL_CAP_POWER_STATUS:
			sb_str(&b, "Power information is unavailable because no verified power backend is active."); break;
		case CELL_CAP_STORAGE_LIST:
			sb_str(&b, "Storage information is unavailable because no verified storage backend is active."); break;
		default:
			sb_str(&b, "That Cell capability is unavailable."); break;
		}
		return b.ok;
	}

	switch (r->id) {
	case CELL_CAP_MEMORY_STATUS:
		sb_str(&b, "Memory: "); sb_u64(&b, r->usable_mib);
		sb_str(&b, " MiB usable; "); sb_u64(&b, r->free_mib);
		sb_str(&b, " MiB remain available to Cell OS.");
		break;
	case CELL_CAP_CPU_INFO:
		sb_str(&b, "CPU: "); sb_str(&b, r->vendor);
		sb_str(&b, ", family "); sb_u64(&b, r->family);
		sb_str(&b, ", model "); sb_u64(&b, r->model);
		sb_str(&b, ", "); sb_u64(&b, r->logical); sb_str(&b, " logical processors.");
		break;
	case CELL_CAP_SYSTEM_STATUS:
		sb_str(&b, "Cell OS is running. Cortex is ");
		sb_str(&b, r->cortex_state[0] == 'n' ? "not ready" : "ready");
		sb_str(&b, "; "); sb_u64(&b, r->memory_mib);
		sb_str(&b, " MiB usable memory; storage "); sb_str(&b, r->storage); sb_char(&b, '.');
		break;
	case CELL_CAP_STORAGE_LIST:
		sb_str(&b, "Storage: "); sb_u64(&b, r->count);
		sb_str(&b, " verified device is available as "); sb_str(&b, r->primary); sb_char(&b, '.');
		break;
	case CELL_CAP_STORAGE_LIST_DIR:
	case CELL_CAP_STORAGE_PWD:
		sb_str(&b, r->vfs_text);
		break;
	default:
		sb_str(&b, "Cell capability completed.");
		break;
	}
	return b.ok;
}
