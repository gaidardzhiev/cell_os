/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/vfs.h"
#include "core/capability.h"
#include "core/task.h"

typedef struct {
	char *p;
	char *end;
	int ok;
} sb_t;

static void sb_init(sb_t *b, char *dst, size_t cap) {
	b->p = dst;
	b->end = dst + (cap ? cap - 1u : 0u);
	b->ok = cap != 0u;
	if (cap) dst[0] = 0;
}

static void sb_char(sb_t *b, char c) {
	if (!b->ok || b->p >= b->end) {
		b->ok = 0;
		return;
	}
	*b->p++ = c;
	*b->p = 0;
}

static void sb_str(sb_t *b, const char *s) {
	while (s && *s) sb_char(b, *s++);
}

static void sb_u64(sb_t *b, uint64_t v) {
	char tmp[24];
	unsigned n = 0;
	if (!v) {
		sb_char(b, '0');
		return;
	}
	while (v && n < sizeof(tmp)) {
		tmp[n++] = (char)('0' + (v % 10u));
		v /= 10u;
	}
	while (n) sb_char(b, tmp[--n]);
}

static size_t str_len(const char *s) {
	size_t n = 0;
	while (s && s[n]) ++n;
	return n;
}

static int str_eq(const char *a, const char *b) {
	if (!a || !b) return 0;
	while (*a && *b) {
		if (*a++ != *b++) return 0;
	}
	return *a == 0 && *b == 0;
}

static int str_starts(const char *s, const char *prefix) {
	if (!s || !prefix) return 0;
	while (*prefix) {
		if (*s++ != *prefix++) return 0;
	}
	return 1;
}

static void str_copy(char *dst, size_t cap, const char *src) {
	size_t n = 0;
	if (!dst || !cap) return;
	while (src && src[n] && n + 1u < cap) {
		dst[n] = src[n];
		++n;
	}
	dst[n] = 0;
}

static const cell_task_record_t *task_record_by_id(const cell_capability_env_t *env, uint32_t id) {
	if (!env || !env->tasks || !id) return 0;
	for (uint32_t i = 0; i < CELL_TASK_HISTORY; ++i) {
		const cell_task_record_t *r = &env->tasks->history[i];
		if (r->state != CELL_TASK_EMPTY && r->id == id) return r;
	}
	return 0;
}

static const cell_task_record_t *task_record_self(const cell_capability_env_t *env) {
	if (!env || !env->tasks) return 0;
	for (uint32_t i = 0; i < CELL_TASK_HISTORY; ++i) {
		const cell_task_record_t *r = &env->tasks->history[i];
		if (r->state == CELL_TASK_RUNNING) return r;
	}
	return 0;
}

static int parse_u32_component(const char *s, size_t n, uint32_t *value) {
	if (!s || !n || !value) return 0;
	uint64_t v = 0;
	for (size_t i = 0; i < n; ++i) {
		if (s[i] < '0' || s[i] > '9') return 0;
		v = v * 10u + (uint32_t)(s[i] - '0');
		if (v > 0xffffffffu) return 0;
	}
	if (!v) return 0;
	*value = (uint32_t)v;
	return 1;
}

static int proc_record_path(const cell_capability_env_t *env, const char *absolute,
	const cell_task_record_t **record, const char **leaf) {
	if (record) *record = 0;
	if (leaf) *leaf = 0;
	if (!absolute || !str_starts(absolute, "/proc/")) return 0;
	const char *p = absolute + 6;
	const char *slash = p;
	while (*slash && *slash != '/') ++slash;
	if (slash == p) return 0;
	const cell_task_record_t *r = 0;
	if ((size_t)(slash - p) == 4u && p[0]=='s' && p[1]=='e' && p[2]=='l' && p[3]=='f') {
		r = task_record_self(env);
	} else {
		uint32_t id = 0;
		if (!parse_u32_component(p, (size_t)(slash - p), &id)) return 0;
		r = task_record_by_id(env, id);
	}
	if (record) *record = r;
	if (leaf) *leaf = *slash == '/' ? slash + 1 : slash;
	return 1;
}

const char *cell_vfs_status_name(cell_vfs_status_t status) {
	switch (status) {
	case CELL_VFS_OK: return "ok";
	case CELL_VFS_NOT_FOUND: return "not_found";
	case CELL_VFS_NOT_DIR: return "not_dir";
	case CELL_VFS_IS_DIR: return "is_dir";
	case CELL_VFS_READ_ONLY: return "read_only";
	case CELL_VFS_EXISTS: return "exists";
	case CELL_VFS_FULL: return "full";
	case CELL_VFS_INVALID: return "invalid";
	case CELL_VFS_IO: return "io";
	case CELL_VFS_NOT_EMPTY: return "not_empty";
	case CELL_VFS_BUSY: return "busy";
	case CELL_VFS_TOO_LARGE: return "too_large";
	case CELL_VFS_BINARY: return "binary";
	default: return "unknown";
	}
}

int cell_vfs_normalize(const cell_vfs_t *vfs, const char *path,
	char out[CELL_VFS_PATH_MAX]) {
	char input[CELL_VFS_PATH_MAX * 2u];
	size_t in = 0;
	if (!path || !*path) path = ".";

	if (path[0] == '/') {
		while (path[in] && in + 1u < sizeof(input)) {
			input[in] = path[in];
			++in;
		}
		if (path[in]) return 0;
		input[in] = 0;
	} else {
		const char *cwd = (vfs && vfs->cwd[0]) ? vfs->cwd : "/";
		size_t cwd_len = str_len(cwd);
		size_t path_len = str_len(path);
		if (cwd_len + path_len + 2u > sizeof(input)) return 0;
		for (size_t i = 0; i < cwd_len; ++i) input[in++] = cwd[i];
		if (in > 1u && input[in - 1u] != '/') input[in++] = '/';
		for (size_t i = 0; i < path_len; ++i) input[in++] = path[i];
		input[in] = 0;
	}

	char components[16][CELLFS_NAME_MAX + 1u];
	unsigned count = 0;
	size_t i = 0;
	while (input[i]) {
		while (input[i] == '/') ++i;
		if (!input[i]) break;
		char component[CELLFS_NAME_MAX + 1u];
		size_t n = 0;
		while (input[i] && input[i] != '/') {
			if (n >= CELLFS_NAME_MAX) return 0;
			component[n++] = input[i++];
		}
		component[n] = 0;
		if (str_eq(component, ".")) continue;
		if (str_eq(component, "..")) {
			if (count) --count;
			continue;
		}
		if (!n || count >= 16u) return 0;
		for (size_t k = 0; k <= n; ++k) components[count][k] = component[k];
		++count;
	}

	size_t o = 0;
	out[o++] = '/';
	for (unsigned k = 0; k < count; ++k) {
		size_t n = str_len(components[k]);
		if (o + n + (k + 1u < count ? 1u : 0u) >= CELL_VFS_PATH_MAX) return 0;
		for (size_t j = 0; j < n; ++j) out[o++] = components[k][j];
		if (k + 1u < count) out[o++] = '/';
	}
	out[o] = 0;
	return 1;
}

typedef enum {
	VNODE_NONE = 0,
	VNODE_MODEL_CORTEX,
	VNODE_PROC_MEMINFO,
	VNODE_DEV_CONSOLE,
	VNODE_DEV_NULL,
	VNODE_DEV_ZERO,
	VNODE_DEV_ATA0,
	VNODE_SYS_CPU_INFO,
	VNODE_SYS_MEMORY_INFO,
	VNODE_SYS_STORAGE_ATA0,
	VNODE_SYS_CORTEX_STATUS,
	VNODE_SYS_CORTEX_MODEL
} vnode_id_t;

typedef struct {
	const char *path;
	const char *listing;
} vdir_def_t;

static const vdir_def_t vdirs[] = {
	{"/", "home/  programs/  models/  proc/  dev/  sys/"},
	{"/models", "cortex"},
	{"/proc", 0},
	{"/dev", "console  null  zero  ata0"},
	{"/sys", "cpu/  memory/  storage/  cortex/"},
	{"/sys/cpu", "info"},
	{"/sys/memory", "info"},
	{"/sys/storage", "ata0"},
	{"/sys/cortex", "status  model"}
};

static const struct { const char *path; vnode_id_t id; } vnodes[] = {
	{"/models/cortex", VNODE_MODEL_CORTEX},
	{"/proc/meminfo", VNODE_PROC_MEMINFO},
	{"/dev/console", VNODE_DEV_CONSOLE},
	{"/dev/null", VNODE_DEV_NULL},
	{"/dev/zero", VNODE_DEV_ZERO},
	{"/dev/ata0", VNODE_DEV_ATA0},
	{"/sys/cpu/info", VNODE_SYS_CPU_INFO},
	{"/sys/memory/info", VNODE_SYS_MEMORY_INFO},
	{"/sys/storage/ata0", VNODE_SYS_STORAGE_ATA0},
	{"/sys/cortex/status", VNODE_SYS_CORTEX_STATUS},
	{"/sys/cortex/model", VNODE_SYS_CORTEX_MODEL}
};

static int is_static_virtual_dir(const char *path) {
	for (size_t i = 0; i < sizeof(vdirs) / sizeof(vdirs[0]); ++i)
		if (str_eq(path, vdirs[i].path)) return 1;
	return 0;
}

static const char *static_dir_listing(const char *path) {
	for (size_t i = 0; i < sizeof(vdirs) / sizeof(vdirs[0]); ++i)
		if (str_eq(path, vdirs[i].path)) return vdirs[i].listing;
	return 0;
}

static vnode_id_t virtual_node_id(const char *path) {
	for (size_t i = 0; i < sizeof(vnodes) / sizeof(vnodes[0]); ++i)
		if (str_eq(path, vnodes[i].path)) return vnodes[i].id;
	return VNODE_NONE;
}

static int is_virtual_dir(const cell_capability_env_t *env, const char *path) {
	if (is_static_virtual_dir(path)) return 1;
	const cell_task_record_t *r = 0;
	const char *leaf = 0;
	return proc_record_path(env, path, &r, &leaf) && r && leaf && !*leaf;
}

static int is_device_stream(vnode_id_t id) {
	return id == VNODE_DEV_NULL || id == VNODE_DEV_ZERO || id == VNODE_DEV_CONSOLE;
}

static int is_persistent_path(const char *path) {
	return str_eq(path, "/home") || str_starts(path, "/home/") ||
		str_eq(path, "/programs") || str_starts(path, "/programs/");
}

static cell_vfs_status_t resolve_persistent(cell_vfs_t *vfs,
	const char *absolute, uint32_t *inode_id) {
	if (!vfs || !vfs->mounted || !absolute || !inode_id || !is_persistent_path(absolute))
		return CELL_VFS_NOT_FOUND;
	uint32_t current = 1u;
	size_t i = 1u;
	while (absolute[i]) {
		char name[CELLFS_NAME_MAX + 1u];
		size_t n = 0;
		while (absolute[i] && absolute[i] != '/') {
			if (n >= CELLFS_NAME_MAX) return CELL_VFS_INVALID;
			name[n++] = absolute[i++];
		}
		name[n] = 0;
		while (absolute[i] == '/') ++i;
		uint32_t child = 0;
		if (!cellfs_find_child(&vfs->fs, current, name, &child)) return CELL_VFS_NOT_FOUND;
		current = child;
		if (absolute[i]) {
			const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, current);
			if (!ino || ino->type != CELLFS_TYPE_DIR) return CELL_VFS_NOT_DIR;
		}
	}
	*inode_id = current;
	return CELL_VFS_OK;
}

static cell_vfs_status_t split_parent(cell_vfs_t *vfs, const char *path,
	char parent[CELL_VFS_PATH_MAX], char name[CELLFS_NAME_MAX + 1u]) {
	char absolute[CELL_VFS_PATH_MAX];
	if (!cell_vfs_normalize(vfs, path, absolute) || str_eq(absolute, "/")) return CELL_VFS_INVALID;
	size_t n = str_len(absolute);
	size_t slash = n;
	while (slash && absolute[slash - 1u] != '/') --slash;
	size_t name_len = n - slash;
	if (!name_len || name_len > CELLFS_NAME_MAX) return CELL_VFS_INVALID;
	for (size_t i = 0; i < name_len; ++i) name[i] = absolute[slash + i];
	name[name_len] = 0;
	if (slash <= 1u) {
		parent[0] = '/';
		parent[1] = 0;
	} else {
		for (size_t i = 0; i < slash - 1u; ++i) parent[i] = absolute[i];
		parent[slash - 1u] = 0;
	}
	return CELL_VFS_OK;
}

static cell_vfs_status_t capability_text(const cell_capability_env_t *env,
	cell_capability_id_t id, char *out, size_t cap) {
	cell_capability_result_t r;
	if (!env || !cell_capability_execute(env, id, &r)) return CELL_VFS_IO;
	return cell_capability_human_response(&r, out, cap) ? CELL_VFS_OK : CELL_VFS_IO;
}

static cell_vfs_status_t proc_list(const cell_capability_env_t *env, char *out, size_t cap) {
	sb_t b; sb_init(&b, out, cap);
	sb_str(&b, "meminfo");
	if (task_record_self(env)) sb_str(&b, "  self/");
	if (env && env->tasks) {
		uint32_t min_id = env->tasks->next_id > CELL_TASK_HISTORY ? env->tasks->next_id - CELL_TASK_HISTORY : 1u;
		for (uint32_t id = min_id; id < env->tasks->next_id; ++id) {
			if (!task_record_by_id(env, id)) continue;
			sb_str(&b, "  "); sb_u64(&b, id); sb_char(&b, '/');
		}
	}
	return b.ok ? CELL_VFS_OK : CELL_VFS_TOO_LARGE;
}

static cell_vfs_status_t proc_record_text(const cell_capability_env_t *env, const char *absolute,
	char *out, size_t cap) {
	const cell_task_record_t *r = 0; const char *leaf = 0;
	if (!proc_record_path(env, absolute, &r, &leaf) || !r) return CELL_VFS_NOT_FOUND;
	if (!leaf || !*leaf) return CELL_VFS_IS_DIR;
	if (str_eq(leaf, "command")) { sb_t b; sb_init(&b, out, cap); sb_str(&b, r->path); sb_char(&b, '\n'); return b.ok ? CELL_VFS_OK : CELL_VFS_TOO_LARGE; }
	if (!str_eq(leaf, "status")) return CELL_VFS_NOT_FOUND;
	return cell_task_describe(env->tasks, r->id, out, cap) ? CELL_VFS_OK : CELL_VFS_TOO_LARGE;
}

static cell_vfs_status_t proc_meminfo(const cell_capability_env_t *env, char *out, size_t cap) {
	return capability_text(env, CELL_CAP_MEMORY_STATUS, out, cap);
}

static cell_vfs_status_t model_text(cell_vfs_t *vfs, char *out, size_t cap) {
	sb_t b; sb_init(&b, out, cap);
	sb_str(&b, "Cortex model: "); sb_u64(&b, vfs->model_bytes); sb_str(&b, " bytes, vocab ");
	sb_u64(&b, vfs->model_vocab); sb_str(&b, ", context "); sb_u64(&b, vfs->model_context);
	sb_str(&b, ", d_model "); sb_u64(&b, vfs->model_d_model); sb_str(&b, ", layers ");
	sb_u64(&b, vfs->model_layers); sb_char(&b, '.');
	return b.ok ? CELL_VFS_OK : CELL_VFS_TOO_LARGE;
}

static cell_vfs_status_t sys_text(cell_vfs_t *vfs, const cell_capability_env_t *env,
	vnode_id_t id, char *out, size_t cap) {
	switch (id) {
	case VNODE_SYS_CPU_INFO: return capability_text(env, CELL_CAP_CPU_INFO, out, cap);
	case VNODE_SYS_MEMORY_INFO: return capability_text(env, CELL_CAP_MEMORY_STATUS, out, cap);
	case VNODE_SYS_STORAGE_ATA0: return capability_text(env, CELL_CAP_STORAGE_LIST, out, cap);
	case VNODE_SYS_CORTEX_STATUS: return capability_text(env, CELL_CAP_SYSTEM_STATUS, out, cap);
	case VNODE_SYS_CORTEX_MODEL: return model_text(vfs, out, cap);
	default: return CELL_VFS_NOT_FOUND;
	}
}

static cell_vfs_status_t dev_text(const cell_capability_env_t *env, vnode_id_t id,
	char *out, size_t cap) {
	switch (id) {
	case VNODE_DEV_NULL: out[0] = 0; return CELL_VFS_OK;
	case VNODE_DEV_ZERO: out[0] = 0; return CELL_VFS_BINARY;
	case VNODE_DEV_CONSOLE: str_copy(out, cap, "Cell OS console device.\n"); return CELL_VFS_OK;
	case VNODE_DEV_ATA0:
		if (!env || !env->ata0_ready) return CELL_VFS_NOT_FOUND;
		str_copy(out, cap, "ata0: verified ATA PIO storage device.\n"); return CELL_VFS_OK;
	default: return CELL_VFS_NOT_FOUND;
	}
}

int cell_vfs_mount(cell_vfs_t *vfs, const cellfs_disk_t *disk,
	uint64_t model_bytes, uint32_t vocab, uint32_t context, uint32_t d_model, uint32_t layers) {
	if (!vfs || !disk) return 0;
	vfs->cwd[0] = '/';
	vfs->cwd[1] = 0;
	vfs->model_bytes = model_bytes;
	vfs->model_vocab = vocab;
	vfs->model_context = context;
	vfs->model_d_model = d_model;
	vfs->model_layers = layers;
	vfs->mounted = 0;
	if (!cellfs_mount(&vfs->fs, disk)) return 0;
	vfs->mounted = 1;
	return 1;
}

cell_vfs_status_t cell_vfs_pwd(const cell_vfs_t *vfs, char *out, size_t cap) {
	if (!vfs || !vfs->mounted || !out || !cap) return CELL_VFS_INVALID;
	str_copy(out, cap, vfs->cwd);
	return CELL_VFS_OK;
}

static cell_vfs_status_t list_persistent(cell_vfs_t *vfs, uint32_t id,
	char *out, size_t cap) {
	const cellfs_inode_t *dir = cellfs_inode(&vfs->fs, id);
	if (!dir) return CELL_VFS_NOT_FOUND;
	if (dir->type != CELLFS_TYPE_DIR) return CELL_VFS_NOT_DIR;
	sb_t b;
	sb_init(&b, out, cap);
	unsigned count = 0;
	for (uint32_t i = 0; i < CELLFS_MAX_INODES; ++i) {
		const cellfs_inode_t *ino = &vfs->fs.inodes[i];
		if (ino->type == CELLFS_TYPE_FREE || ino->parent != id || ino->id == id) continue;
		if (count++) sb_str(&b, "  ");
		sb_str(&b, ino->name);
		if (ino->type == CELLFS_TYPE_DIR) sb_char(&b, '/');
	}
	if (!count) sb_str(&b, "(empty)");
	return b.ok ? CELL_VFS_OK : CELL_VFS_TOO_LARGE;
}

cell_vfs_status_t cell_vfs_list(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, char *out, size_t cap) {
	char absolute[CELL_VFS_PATH_MAX];
	if (!vfs || !vfs->mounted || !cell_vfs_normalize(vfs, path, absolute)) return CELL_VFS_INVALID;
	if (str_eq(absolute, "/proc")) return proc_list(env, out, cap);
	const char *listing = static_dir_listing(absolute);
	if (listing) { str_copy(out, cap, listing); return CELL_VFS_OK; }
	{
		const cell_task_record_t *r = 0; const char *leaf = 0;
		if (proc_record_path(env, absolute, &r, &leaf)) {
			if (!r) return CELL_VFS_NOT_FOUND;
			if (leaf && !*leaf) { str_copy(out, cap, "status  command"); return CELL_VFS_OK; }
			return CELL_VFS_NOT_DIR;
		}
	}
	uint32_t id = 0;
	cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
	if (status != CELL_VFS_OK) return status;
	return list_persistent(vfs, id, out, cap);
}

cell_vfs_status_t cell_vfs_chdir(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, char *out, size_t cap) {
	char absolute[CELL_VFS_PATH_MAX];
	if (!vfs || !vfs->mounted || !cell_vfs_normalize(vfs, path, absolute)) return CELL_VFS_INVALID;
	if (!is_virtual_dir(env, absolute)) {
		uint32_t id = 0;
		cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
		if (status != CELL_VFS_OK) return status;
		const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
		if (!ino || ino->type != CELLFS_TYPE_DIR) return CELL_VFS_NOT_DIR;
	}
	str_copy(vfs->cwd, sizeof(vfs->cwd), absolute);
	if (out && cap) str_copy(out, cap, absolute);
	return CELL_VFS_OK;
}

cell_vfs_status_t cell_vfs_cat(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, char *out, size_t cap) {
	char absolute[CELL_VFS_PATH_MAX];
	if (!vfs || !vfs->mounted || !cell_vfs_normalize(vfs, path, absolute) || !out || !cap)
		return CELL_VFS_INVALID;
	if (is_virtual_dir(env, absolute)) return CELL_VFS_IS_DIR;
	vnode_id_t node = virtual_node_id(absolute);
	if (node == VNODE_PROC_MEMINFO) return proc_meminfo(env, out, cap);
	if (node == VNODE_MODEL_CORTEX) return model_text(vfs, out, cap);
	if (node >= VNODE_DEV_CONSOLE && node <= VNODE_DEV_ATA0) return dev_text(env, node, out, cap);
	if (node >= VNODE_SYS_CPU_INFO) return sys_text(vfs, env, node, out, cap);
	if (str_starts(absolute, "/proc/")) return proc_record_text(env, absolute, out, cap);
	if (str_starts(absolute, "/models/") || str_starts(absolute, "/proc/") ||
	    str_starts(absolute, "/dev/") || str_starts(absolute, "/sys/")) return CELL_VFS_NOT_FOUND;

	uint32_t id = 0;
	cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
	if (status != CELL_VFS_OK) return status;
	const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
	if (!ino) return CELL_VFS_NOT_FOUND;
	if (ino->type == CELLFS_TYPE_DIR) return CELL_VFS_IS_DIR;
	if ((size_t)ino->size + 1u > cap) return CELL_VFS_TOO_LARGE;
	size_t bytes = 0;
	if (!cellfs_read_file(&vfs->fs, id, out, cap - 1u, &bytes)) return CELL_VFS_IO;
	for (size_t i = 0; i < bytes; ++i) {
		unsigned char c = (unsigned char)out[i];
		if (!((c >= 32u && c <= 126u) || c == '\n' || c == '\r' || c == '\t')) {
			out[0] = 0;
			return CELL_VFS_BINARY;
		}
	}
	out[bytes] = 0;
	return CELL_VFS_OK;
}

static cell_vfs_status_t create_common(cell_vfs_t *vfs, const char *path, int directory);

static cell_vfs_status_t virtual_read_text(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *absolute, char *out, size_t cap, size_t *bytes_out) {
	cell_vfs_status_t status = cell_vfs_cat(vfs, env, absolute, out, cap);
	if (status != CELL_VFS_OK) return status;
	if (bytes_out) *bytes_out = str_len(out);
	return CELL_VFS_OK;
}

cell_vfs_status_t cell_vfs_open_file(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, uint32_t mode, char absolute[CELL_VFS_PATH_MAX], size_t *size_out) {
	if (size_out) *size_out = 0;
	if (!vfs || !vfs->mounted || !absolute || !cell_vfs_normalize(vfs, path, absolute))
		return CELL_VFS_INVALID;
	vnode_id_t node = virtual_node_id(absolute);
	if (!(mode & (CELL_VFS_OPEN_READ | CELL_VFS_OPEN_WRITE))) return CELL_VFS_INVALID;
	if (mode & ~(CELL_VFS_OPEN_READ | CELL_VFS_OPEN_WRITE | CELL_VFS_OPEN_CREATE | CELL_VFS_OPEN_TRUNCATE))
		return CELL_VFS_INVALID;

	if (is_persistent_path(absolute)) {
		uint32_t id = 0;
		cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
		if (status == CELL_VFS_NOT_FOUND && (mode & CELL_VFS_OPEN_CREATE)) {
			status = create_common(vfs, absolute, 0);
			if (status != CELL_VFS_OK) return status;
			status = resolve_persistent(vfs, absolute, &id);
		}
		if (status != CELL_VFS_OK) return status;
		const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
		if (!ino) return CELL_VFS_NOT_FOUND;
		if (ino->type == CELLFS_TYPE_DIR) return CELL_VFS_IS_DIR;
		if ((mode & CELL_VFS_OPEN_TRUNCATE) && (mode & CELL_VFS_OPEN_WRITE)) {
			static const uint8_t empty = 0;
			if (!cellfs_write_file(&vfs->fs, id, &empty, 0, 0)) return CELL_VFS_IO;
			ino = cellfs_inode(&vfs->fs, id);
		}
		if (size_out) *size_out = ino ? ino->size : 0u;
		return CELL_VFS_OK;
	}

	if (is_device_stream(node)) {
		if (mode & (CELL_VFS_OPEN_CREATE | CELL_VFS_OPEN_TRUNCATE)) return CELL_VFS_INVALID;
		if (size_out) *size_out = 0u;
		return CELL_VFS_OK;
	}
	if (mode & (CELL_VFS_OPEN_WRITE | CELL_VFS_OPEN_CREATE | CELL_VFS_OPEN_TRUNCATE))
		return CELL_VFS_READ_ONLY;
	if (is_virtual_dir(env, absolute)) return CELL_VFS_IS_DIR;
	char tmp[CELL_VFS_TEXT_MAX];
	size_t bytes = 0;
	cell_vfs_status_t status = virtual_read_text(vfs, env, absolute, tmp, sizeof(tmp), &bytes);
	if (status != CELL_VFS_OK) return status;
	if (size_out) *size_out = bytes;
	return CELL_VFS_OK;
}

cell_vfs_status_t cell_vfs_read_at(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, size_t offset, void *dst, size_t cap, size_t *bytes_out) {
	if (bytes_out) *bytes_out = 0;
	if (!vfs || !vfs->mounted || (!dst && cap)) return CELL_VFS_INVALID;
	char absolute[CELL_VFS_PATH_MAX];
	if (!cell_vfs_normalize(vfs, path, absolute)) return CELL_VFS_INVALID;
	vnode_id_t node = virtual_node_id(absolute);

	if (is_persistent_path(absolute)) {
		uint32_t id = 0;
		cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
		if (status != CELL_VFS_OK) return status;
		const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
		if (!ino) return CELL_VFS_NOT_FOUND;
		if (ino->type == CELLFS_TYPE_DIR) return CELL_VFS_IS_DIR;
		if (offset >= ino->size || !cap) return CELL_VFS_OK;
		size_t total = 0;
		if (!cellfs_read_file(&vfs->fs, id, vfs->fs.scratch, sizeof(vfs->fs.scratch), &total))
			return CELL_VFS_IO;
		size_t n = total - offset;
		if (n > cap) n = cap;
		uint8_t *d = (uint8_t *)dst;
		for (size_t i = 0; i < n; ++i) d[i] = vfs->fs.scratch[offset + i];
		if (bytes_out) *bytes_out = n;
		return CELL_VFS_OK;
	}

	if (node == VNODE_DEV_NULL || node == VNODE_DEV_CONSOLE) return CELL_VFS_OK;
	if (node == VNODE_DEV_ZERO) {
		uint8_t *d = (uint8_t *)dst;
		for (size_t i = 0; i < cap; ++i) d[i] = 0u;
		if (bytes_out) *bytes_out = cap;
		return CELL_VFS_OK;
	}
	if (is_virtual_dir(env, absolute)) return CELL_VFS_IS_DIR;
	char tmp[CELL_VFS_TEXT_MAX];
	size_t total = 0;
	cell_vfs_status_t status = virtual_read_text(vfs, env, absolute, tmp, sizeof(tmp), &total);
	if (status != CELL_VFS_OK) return status;
	if (offset >= total || !cap) return CELL_VFS_OK;
	size_t n = total - offset;
	if (n > cap) n = cap;
	uint8_t *d = (uint8_t *)dst;
	for (size_t i = 0; i < n; ++i) d[i] = (uint8_t)tmp[offset + i];
	if (bytes_out) *bytes_out = n;
	return CELL_VFS_OK;
}

cell_vfs_status_t cell_vfs_write_at(cell_vfs_t *vfs, const char *path, size_t offset,
	const void *data, size_t bytes, size_t *size_out) {
	if (size_out) *size_out = 0;
	if (!vfs || !vfs->mounted || (!data && bytes)) return CELL_VFS_INVALID;
	char absolute[CELL_VFS_PATH_MAX];
	if (!cell_vfs_normalize(vfs, path, absolute)) return CELL_VFS_INVALID;
	if (!is_persistent_path(absolute)) return CELL_VFS_READ_ONLY;
	uint32_t id = 0;
	cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
	if (status != CELL_VFS_OK) return status;
	const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
	if (!ino) return CELL_VFS_NOT_FOUND;
	if (ino->type == CELLFS_TYPE_DIR) return CELL_VFS_IS_DIR;
	if (offset > CELLFS_FILE_MAX || bytes > CELLFS_FILE_MAX - offset) return CELL_VFS_TOO_LARGE;

	size_t old_size = ino->size;
	if (old_size) {
		size_t got = 0;
		if (!cellfs_read_file(&vfs->fs, id, vfs->fs.scratch, sizeof(vfs->fs.scratch), &got) || got != old_size)
			return CELL_VFS_IO;
	}
	if (offset > old_size) {
		for (size_t i = old_size; i < offset; ++i) vfs->fs.scratch[i] = 0;
	}
	const uint8_t *src = (const uint8_t *)data;
	for (size_t i = 0; i < bytes; ++i) vfs->fs.scratch[offset + i] = src[i];
	size_t total = old_size;
	if (offset + bytes > total) total = offset + bytes;
	static const uint8_t empty = 0;
	const void *payload = total ? (const void *)vfs->fs.scratch : (const void *)&empty;
	if (!cellfs_write_file(&vfs->fs, id, payload, total, 0)) return CELL_VFS_FULL;
	if (size_out) *size_out = total;
	return CELL_VFS_OK;
}

cell_vfs_status_t cell_vfs_read_bytes(cell_vfs_t *vfs, const char *path,
	void *dst, size_t cap, size_t *bytes_out) {
	char absolute[CELL_VFS_PATH_MAX];
	if (bytes_out) *bytes_out = 0;
	if (!vfs || !vfs->mounted || !dst || !cell_vfs_normalize(vfs, path, absolute)) return CELL_VFS_INVALID;
	if (!is_persistent_path(absolute)) return CELL_VFS_READ_ONLY;
	uint32_t id = 0;
	cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
	if (status != CELL_VFS_OK) return status;
	const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
	if (!ino) return CELL_VFS_NOT_FOUND;
	if (ino->type == CELLFS_TYPE_DIR) return CELL_VFS_IS_DIR;
	if ((size_t)ino->size > cap) return CELL_VFS_TOO_LARGE;
	size_t bytes = 0;
	if (!cellfs_read_file(&vfs->fs, id, dst, cap, &bytes)) return CELL_VFS_IO;
	if (bytes_out) *bytes_out = bytes;
	return CELL_VFS_OK;
}

static cell_vfs_status_t create_common(cell_vfs_t *vfs, const char *path, int directory) {
	char parent[CELL_VFS_PATH_MAX];
	char name[CELLFS_NAME_MAX + 1u];
	cell_vfs_status_t status = split_parent(vfs, path, parent, name);
	if (status != CELL_VFS_OK) return status;
	if (!is_persistent_path(parent)) return CELL_VFS_READ_ONLY;
	uint32_t parent_id = 0;
	status = resolve_persistent(vfs, parent, &parent_id);
	if (status != CELL_VFS_OK) return status;
	uint32_t existing = 0;
	if (cellfs_find_child(&vfs->fs, parent_id, name, &existing)) return CELL_VFS_EXISTS;
	int ok = directory ? cellfs_create_dir(&vfs->fs, parent_id, name, 0) :
		cellfs_create_file(&vfs->fs, parent_id, name, 0);
	return ok ? CELL_VFS_OK : CELL_VFS_FULL;
}

cell_vfs_status_t cell_vfs_mkdir(cell_vfs_t *vfs, const char *path) {
	return create_common(vfs, path, 1);
}

cell_vfs_status_t cell_vfs_touch(cell_vfs_t *vfs, const char *path) {
	return create_common(vfs, path, 0);
}

cell_vfs_status_t cell_vfs_write_bytes(cell_vfs_t *vfs, const char *path,
	const void *data, size_t bytes, int append) {
	if (!vfs || (!data && bytes)) return CELL_VFS_INVALID;
	char absolute[CELL_VFS_PATH_MAX];
	if (!cell_vfs_normalize(vfs, path, absolute)) return CELL_VFS_INVALID;
	if (!is_persistent_path(absolute)) return CELL_VFS_READ_ONLY;
	uint32_t id = 0;
	cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
	if (status == CELL_VFS_NOT_FOUND) {
		status = create_common(vfs, absolute, 0);
		if (status != CELL_VFS_OK) return status;
		status = resolve_persistent(vfs, absolute, &id);
	}
	if (status != CELL_VFS_OK) return status;
	const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
	if (!ino || ino->type != CELLFS_TYPE_FILE) return CELL_VFS_IS_DIR;
	if ((append ? (size_t)ino->size : 0u) + bytes > CELLFS_FILE_MAX) return CELL_VFS_TOO_LARGE;
	return cellfs_write_file(&vfs->fs, id, data, bytes, append) ? CELL_VFS_OK : CELL_VFS_FULL;
}

cell_vfs_status_t cell_vfs_write(cell_vfs_t *vfs, const char *path,
	const char *text, int append) {
	if (!text) return CELL_VFS_INVALID;
	return cell_vfs_write_bytes(vfs, path, text, str_len(text), append);
}

cell_vfs_status_t cell_vfs_remove(cell_vfs_t *vfs, const char *path, int directory) {
	char absolute[CELL_VFS_PATH_MAX];
	if (!vfs || !cell_vfs_normalize(vfs, path, absolute)) return CELL_VFS_INVALID;
	if (!is_persistent_path(absolute)) return CELL_VFS_READ_ONLY;
	if (str_eq(absolute, "/home") || str_eq(absolute, "/programs")) return CELL_VFS_READ_ONLY;
	if (directory) {
		size_t n = str_len(absolute);
		if (str_eq(vfs->cwd, absolute) ||
		    (str_starts(vfs->cwd, absolute) && vfs->cwd[n] == '/')) return CELL_VFS_BUSY;
	}
	uint32_t id = 0;
	cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
	if (status != CELL_VFS_OK) return status;
	const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
	if (!ino) return CELL_VFS_NOT_FOUND;
	if (directory && ino->type != CELLFS_TYPE_DIR) return CELL_VFS_NOT_DIR;
	if (!directory && ino->type != CELLFS_TYPE_FILE) return CELL_VFS_IS_DIR;
	if (directory) {
		for (uint32_t i = 0; i < CELLFS_MAX_INODES; ++i) {
			if (vfs->fs.inodes[i].type != CELLFS_TYPE_FREE && vfs->fs.inodes[i].parent == id)
				return CELL_VFS_NOT_EMPTY;
		}
	}
	return cellfs_remove(&vfs->fs, id, directory) ? CELL_VFS_OK : CELL_VFS_IO;
}

cell_vfs_status_t cell_vfs_stat(cell_vfs_t *vfs, const cell_capability_env_t *env,
	const char *path, char *out, size_t cap) {
	char absolute[CELL_VFS_PATH_MAX];
	if (!vfs || !cell_vfs_normalize(vfs, path, absolute) || !out || !cap) return CELL_VFS_INVALID;
	sb_t b;
	sb_init(&b, out, cap);
	if (is_virtual_dir(env, absolute)) {
		sb_str(&b, "virtual directory ");
		sb_str(&b, absolute);
		return b.ok ? CELL_VFS_OK : CELL_VFS_TOO_LARGE;
	}
	if (str_starts(absolute, "/models/") || str_starts(absolute, "/proc/") ||
	    str_starts(absolute, "/dev/") || str_starts(absolute, "/sys/")) {
		char tmp[CELL_VFS_TEXT_MAX];
		cell_vfs_status_t status = cell_vfs_cat(vfs, env, absolute, tmp, sizeof(tmp));
		if (status != CELL_VFS_OK) return status;
		sb_str(&b, "virtual node ");
		sb_str(&b, absolute);
		sb_str(&b, " (live)");
		return b.ok ? CELL_VFS_OK : CELL_VFS_TOO_LARGE;
	}
	uint32_t id = 0;
	cell_vfs_status_t status = resolve_persistent(vfs, absolute, &id);
	if (status != CELL_VFS_OK) return status;
	const cellfs_inode_t *ino = cellfs_inode(&vfs->fs, id);
	if (!ino) return CELL_VFS_NOT_FOUND;
	sb_str(&b, ino->type == CELLFS_TYPE_DIR ? "directory " : "file ");
	sb_str(&b, absolute);
	if (ino->type == CELLFS_TYPE_FILE) {
		sb_str(&b, " size=");
		sb_u64(&b, ino->size);
	}
	sb_str(&b, " generation=");
	sb_u64(&b, ino->generation);
	return b.ok ? CELL_VFS_OK : CELL_VFS_TOO_LARGE;
}
