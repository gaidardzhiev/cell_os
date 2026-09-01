/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/task.h"
#include "core/capability.h"
#include "core/vfs.h"
#include "core/syscall.h"

typedef struct {
	char *p;
	char *end;
	int ok;
} outbuf_t;

static void ob_init(outbuf_t *b, char *out, size_t cap) {
	b->p = out;
	b->end = out + (cap ? cap - 1u : 0u);
	b->ok = cap != 0u;
	if (cap) out[0] = 0;
}
static void ob_char(outbuf_t *b, char c) {
	if (!b->ok || b->p >= b->end) { b->ok = 0; return; }
	*b->p++ = c; *b->p = 0;
}
static void ob_str(outbuf_t *b, const char *s) { while (s && *s) ob_char(b, *s++); }
static void ob_u32(outbuf_t *b, uint32_t v) {
	char tmp[12]; unsigned n = 0;
	if (!v) { ob_char(b, '0'); return; }
	while (v && n < sizeof(tmp)) { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; }
	while (n) ob_char(b, tmp[--n]);
}
static void ob_i32(outbuf_t *b, int32_t v) {
	if (v < 0) { ob_char(b, '-'); ob_u32(b, (uint32_t)(-(int64_t)v)); }
	else ob_u32(b, (uint32_t)v);
}
static void copy_text(char *dst, size_t cap, const char *src) {
	if (!dst || !cap) return;
	size_t n = 0;
	while (src && src[n] && n + 1u < cap) { dst[n] = src[n]; ++n; }
	dst[n] = 0;
}
static void zero_bytes(void *dst, size_t n) {
	uint8_t *p = (uint8_t *)dst; while (n--) *p++ = 0;
}
static size_t text_len(const char *s) {
	size_t n = 0; while (s && s[n]) ++n; return n;
}

const char *cell_task_state_name(cell_task_state_t state) {
	switch (state) {
	case CELL_TASK_EMPTY: return "empty";
	case CELL_TASK_READY: return "ready";
	case CELL_TASK_RUNNING: return "running";
	case CELL_TASK_EXITED: return "exited";
	case CELL_TASK_FAULTED: return "faulted";
	default: return "unknown";
	}
}
const char *cell_task_fault_name(cell_task_fault_t fault) {
	switch (fault) {
	case CELL_TASK_FAULT_NONE: return "none";
	case CELL_TASK_FAULT_LOAD: return "load";
	case CELL_TASK_FAULT_PERMISSION: return "permission";
	case CELL_TASK_FAULT_GAS: return "gas";
	case CELL_TASK_FAULT_PC: return "pc";
	case CELL_TASK_FAULT_MEMORY: return "memory";
	case CELL_TASK_FAULT_CAPABILITY: return "capability";
	case CELL_TASK_FAULT_OUTPUT: return "output";
	case CELL_TASK_FAULT_ARGUMENT: return "argument";
	case CELL_TASK_FAULT_ARITHMETIC: return "arithmetic";
	case CELL_TASK_FAULT_STACK: return "stack";
	case CELL_TASK_FAULT_INSTRUCTION: return "instruction";
	default: return "unknown";
	}
}

static uint64_t all_known_caps(void) {
	uint64_t m = 0;
	for (unsigned id = CELL_CAP_SYSTEM_STATUS; id <= CELL_CAP_POWER_STATUS; ++id) m |= 1ull << id;
	return m;
}

uint64_t cell_task_default_policy(void) {
	return (1ull << CELL_CAP_SYSTEM_STATUS) |
		(1ull << CELL_CAP_CPU_INFO) |
		(1ull << CELL_CAP_MEMORY_STATUS) |
		(1ull << CELL_CAP_STORAGE_LIST) |
		(1ull << CELL_CAP_STORAGE_LIST_DIR) |
		(1ull << CELL_CAP_STORAGE_PWD);
}

void cell_task_manager_init(cell_task_manager_t *tm, uint64_t policy_mask) {
	if (!tm) return;
	zero_bytes(tm, sizeof(*tm));
	tm->next_id = 1u;
	tm->policy_mask = policy_mask & all_known_caps();
}

static cell_task_record_t *new_record(cell_task_manager_t *tm, const char *path) {
	uint32_t slot = tm->next_slot++ % CELL_TASK_HISTORY;
	cell_task_record_t *r = &tm->history[slot];
	zero_bytes(r, sizeof(*r));
	r->id = tm->next_id++;
	if (!tm->next_id) tm->next_id = 1u;
	r->state = CELL_TASK_READY;
	copy_text(r->path, sizeof(r->path), path);
	return r;
}

static int output_char_ok(uint8_t c) {
	return (c >= 32u && c <= 126u) || c == '\n' || c == '\r' || c == '\t';
}

static int output_bytes(outbuf_t *b, const uint8_t *p, uint32_t n) {
	for (uint32_t i = 0; i < n; ++i) {
		uint8_t c = p[i];
		if (!output_char_ok(c)) return 0;
		ob_char(b, (char)c);
		if (!b->ok) return 0;
	}
	return 1;
}

static int pointer_span(const cell_exec_t *exec, uint8_t *memory,
	uint64_t addr, uint32_t bytes, int writable, uint8_t **ptr) {
	if (addr >= CELL_EXEC_DATA_BASE) {
		uint64_t off = addr - CELL_EXEC_DATA_BASE;
		if (writable || off > exec->h->data_bytes || bytes > exec->h->data_bytes - off) return 0;
		if (ptr) *ptr = (uint8_t *)(uintptr_t)(exec->data + (uint32_t)off);
		return 1;
	}
	if (addr > exec->h->memory_bytes || bytes > exec->h->memory_bytes - addr) return 0;
	if (ptr) *ptr = memory + (uint32_t)addr;
	return 1;
}

static int pointer_cstr(const cell_exec_t *exec, uint8_t *memory,
	uint64_t addr, uint32_t *length, const uint8_t **ptr) {
	if (addr >= CELL_EXEC_DATA_BASE) {
		uint64_t off64 = addr - CELL_EXEC_DATA_BASE;
		if (off64 >= exec->h->data_bytes) return 0;
		uint32_t off = (uint32_t)off64;
		for (uint32_t i = off; i < exec->h->data_bytes; ++i) {
			if (exec->data[i] == 0u) {
				if (length) *length = i - off;
				if (ptr) *ptr = exec->data + off;
				return 1;
			}
		}
		return 0;
	}
	if (addr >= exec->h->memory_bytes) return 0;
	for (uint32_t i = (uint32_t)addr; i < exec->h->memory_bytes; ++i) {
		if (memory[i] == 0u) {
			if (length) *length = i - (uint32_t)addr;
			if (ptr) *ptr = memory + (uint32_t)addr;
			return 1;
		}
	}
	return 0;
}

static int copy_pointer_cstr(const cell_exec_t *exec, uint8_t *memory,
	uint64_t addr, char *dst, size_t cap) {
	uint32_t n = 0;
	const uint8_t *src = 0;
	if (!dst || !cap || !pointer_cstr(exec, memory, addr, &n, &src) || (size_t)n + 1u > cap) return 0;
	for (uint32_t i = 0; i < n; ++i) dst[i] = (char)src[i];
	dst[n] = 0;
	return 1;
}

static int output_pointer_string(outbuf_t *b, const cell_exec_t *exec,
	uint8_t *memory, uint64_t addr, int newline) {
	uint32_t n = 0;
	const uint8_t *p = 0;
	if (!pointer_cstr(exec, memory, addr, &n, &p)) return 0;
	if (!output_bytes(b, p, n)) return 0;
	if (newline) ob_char(b, '\n');
	return b->ok;
}

static void fault(cell_task_record_t *r, cell_task_fault_t why) {
	r->state = CELL_TASK_FAULTED;
	r->fault = why;
	r->exit_code = -1;
}

static int append_capability(outbuf_t *b, const cell_capability_env_t *env,
	cell_capability_id_t id, uint64_t *ok_value) {
	cell_capability_result_t result;
	char text[CELL_CAP_RESPONSE_MAX];
	if (!cell_capability_execute(env, id, &result)) return 0;
	if (!cell_capability_human_response(&result, text, sizeof(text))) return 0;
	ob_str(b, text);
	if (!b->ok) return 0;
	if (ok_value) *ok_value = result.ok ? 1u : 0u;
	return 1;
}

static void store_u64(uint8_t *memory, uint32_t off, uint64_t value) {
	for (unsigned i = 0; i < 8u; ++i) memory[off + i] = (uint8_t)(value >> (i * 8u));
}

static uint64_t load_u64(const uint8_t *memory, uint32_t off) {
	uint64_t value = 0;
	for (unsigned i = 0; i < 8u; ++i) value |= (uint64_t)memory[off + i] << (i * 8u);
	return value;
}

static void reset_process_state(cell_task_manager_t *tm) {
	zero_bytes(tm->fds, sizeof(tm->fds));
	zero_bytes(tm->heap, sizeof(tm->heap));
	zero_bytes(tm->calls, sizeof(tm->calls));
	tm->heap_next = 16u;
	tm->call_depth = 0u;
	tm->errno_value = 0;
	tm->fds[CELL_STDIN_FILENO].used = 1u;
	tm->fds[CELL_STDIN_FILENO].readable = 1u;
	tm->fds[CELL_STDOUT_FILENO].used = 1u;
	tm->fds[CELL_STDOUT_FILENO].writable = 1u;
	tm->fds[CELL_STDERR_FILENO].used = 1u;
	tm->fds[CELL_STDERR_FILENO].writable = 1u;
}

static int prepare_argv(cell_task_manager_t *tm, const cell_exec_t *exec,
	uint32_t argc, const char *const argv[]) {
	if (!argc) {
		tm->heap_next = 16u;
		return tm->heap_next <= exec->h->memory_bytes || exec->h->memory_bytes == 0u;
	}
	if (!argv || argc > CELL_TASK_ARGC_MAX) return 0;
	const uint32_t table = 16u;
	uint32_t pos = table + (argc + 1u) * 8u;
	for (uint32_t i = 0; i < argc; ++i) {
		if (!argv[i]) return 0;
		size_t n = text_len(argv[i]);
		if (n > CELL_EXEC_MEMORY_MAX || pos > CELL_EXEC_MEMORY_MAX - (uint32_t)n - 1u) return 0;
		pos += (uint32_t)n + 1u;
	}
	if (pos > exec->h->memory_bytes) {
		if (argc == 1u) return 1;
		return 0;
	}
	pos = table + (argc + 1u) * 8u;
	for (uint32_t i = 0; i < argc; ++i) {
		store_u64(tm->memory, table + i * 8u, pos);
		size_t n = text_len(argv[i]);
		for (size_t j = 0; j < n; ++j) tm->memory[pos + (uint32_t)j] = (uint8_t)argv[i][j];
		tm->memory[pos + (uint32_t)n] = 0;
		pos += (uint32_t)n + 1u;
	}
	store_u64(tm->memory, table + argc * 8u, 0u);
	tm->regs[0] = argc;
	tm->regs[1] = table;
	tm->heap_next = (pos + 7u) & ~7u;
	return tm->heap_next <= exec->h->memory_bytes;
}

static int atoi_memory(const cell_exec_t *exec, uint8_t *memory,
	uint64_t addr, int64_t *value) {
	uint32_t n = 0;
	const uint8_t *src = 0;
	if (!pointer_cstr(exec, memory, addr, &n, &src)) return 0;
	uint32_t i = 0;
	while (i < n && (src[i] == ' ' || src[i] == '\t')) ++i;
	int neg = 0;
	if (i < n && (src[i] == '+' || src[i] == '-')) {
		neg = src[i] == '-'; ++i;
	}
	int64_t v = 0;
	while (i < n) {
		uint8_t c = src[i];
		if (c < '0' || c > '9') break;
		if (v > 2147483647ll / 10ll) { v = 2147483647ll; break; }
		v = v * 10ll + (c - '0');
		if (v > 2147483647ll) { v = 2147483647ll; break; }
		++i;
	}
	*value = neg ? -v : v;
	return 1;
}

static int strcmp_memory_data(const cell_exec_t *exec, uint8_t *memory,
	uint64_t addr, uint32_t data_off, int64_t *value) {
	uint32_t n = 0;
	const uint8_t *src = 0;
	if (!pointer_cstr(exec, memory, addr, &n, &src) || data_off >= exec->h->data_bytes) return 0;
	uint32_t i = 0;
	for (;;) {
		uint8_t a = i <= n ? src[i] : 0u;
		if (data_off + i >= exec->h->data_bytes) return 0;
		uint8_t b = exec->data[data_off + i];
		if (a != b) { *value = a < b ? -1 : 1; return 1; }
		if (!a) { *value = 0; return 1; }
		++i;
	}
}

static int str_starts_task(const char *s, const char *prefix) {
	if (!s || !prefix) return 0;
	while (*prefix) if (*s++ != *prefix++) return 0;
	return 1;
}

static int task_path_writable(const char *absolute) {
	return str_starts_task(absolute, "/home/");
}

static int vfs_errno(cell_vfs_status_t status) {
	switch (status) {
	case CELL_VFS_NOT_FOUND: return CELL_ENOENT;
	case CELL_VFS_NOT_DIR: return CELL_ENOTDIR;
	case CELL_VFS_IS_DIR: return CELL_EISDIR;
	case CELL_VFS_READ_ONLY: return CELL_EROFS;
	case CELL_VFS_EXISTS: return CELL_EEXIST;
	case CELL_VFS_FULL: return CELL_ENOSPC;
	case CELL_VFS_INVALID: return CELL_EINVAL;
	case CELL_VFS_IO: return CELL_EIO;
	case CELL_VFS_NOT_EMPTY: return CELL_ENOTEMPTY;
	case CELL_VFS_TOO_LARGE: return CELL_EFBIG;
	default: return CELL_EIO;
	}
}

static int64_t syscall_fail(cell_task_manager_t *tm, int err) {
	tm->errno_value = err;
	return -1;
}

static int fd_valid(const cell_task_manager_t *tm, int fd) {
	return fd >= 0 && fd < (int)CELL_TASK_FD_MAX && tm->fds[fd].used;
}

static int alloc_fd(cell_task_manager_t *tm) {
	for (int fd = 3; fd < (int)CELL_TASK_FD_MAX; ++fd) if (!tm->fds[fd].used) return fd;
	return -1;
}

static int64_t sys_open(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const cell_exec_t *exec, uint64_t path_addr, uint64_t flags64) {
	char path[CELL_VFS_PATH_MAX];
	if (!copy_pointer_cstr(exec, tm->memory, path_addr, path, sizeof(path))) return syscall_fail(tm, CELL_EFAULT);
	if (flags64 > 0x7fffffffu) return syscall_fail(tm, CELL_EINVAL);
	int flags = (int)flags64;
	int access = flags & CELL_O_ACCMODE;
	if (access != CELL_O_RDONLY && access != CELL_O_WRONLY && access != CELL_O_RDWR)
		return syscall_fail(tm, CELL_EINVAL);
	if (flags & ~(CELL_O_ACCMODE | CELL_O_CREAT | CELL_O_TRUNC | CELL_O_APPEND))
		return syscall_fail(tm, CELL_EINVAL);
	int readable = access == CELL_O_RDONLY || access == CELL_O_RDWR;
	int writable = access == CELL_O_WRONLY || access == CELL_O_RDWR;
	uint32_t mode = readable ? CELL_VFS_OPEN_READ : 0u;
	if (writable) mode |= CELL_VFS_OPEN_WRITE;
	if (flags & CELL_O_CREAT) mode |= CELL_VFS_OPEN_CREATE;
	if (flags & CELL_O_TRUNC) {
		if (!writable) return syscall_fail(tm, CELL_EINVAL);
		mode |= CELL_VFS_OPEN_TRUNCATE;
	}
	char absolute[CELL_VFS_PATH_MAX];
	if (!cell_vfs_normalize(vfs, path, absolute)) return syscall_fail(tm, CELL_EINVAL);
	if (writable && !task_path_writable(absolute)) return syscall_fail(tm, CELL_EACCES);
	size_t size = 0;
	cell_vfs_status_t st = cell_vfs_open_file(vfs, env, absolute, mode, path, &size);
	if (st != CELL_VFS_OK) return syscall_fail(tm, vfs_errno(st));
	int fd = alloc_fd(tm);
	if (fd < 0) return syscall_fail(tm, CELL_EMFILE);
	cell_task_fd_t *f = &tm->fds[fd];
	zero_bytes(f, sizeof(*f));
	f->used = 1u; f->readable = (uint8_t)readable; f->writable = (uint8_t)writable;
	f->append = (uint8_t)((flags & CELL_O_APPEND) != 0);
	f->offset = f->append ? (uint32_t)size : 0u;
	copy_text(f->path, sizeof(f->path), path);
	return fd;
}

static int64_t sys_close(cell_task_manager_t *tm, int64_t fd64) {
	if (fd64 < 0 || fd64 >= (int64_t)CELL_TASK_FD_MAX || !fd_valid(tm, (int)fd64))
		return syscall_fail(tm, CELL_EBADF);
	zero_bytes(&tm->fds[(int)fd64], sizeof(tm->fds[0]));
	return 0;
}

static int64_t sys_read(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const cell_exec_t *exec, int64_t fd64,
	uint64_t addr, uint64_t count64) {
	if (fd64 < 0 || fd64 >= (int64_t)CELL_TASK_FD_MAX || !fd_valid(tm, (int)fd64) || !tm->fds[(int)fd64].readable)
		return syscall_fail(tm, CELL_EBADF);
	if (count64 > 0xffffffffu) return syscall_fail(tm, CELL_EINVAL);
	uint32_t count = (uint32_t)count64;
	uint8_t *dst = 0;
	if (!pointer_span(exec, tm->memory, addr, count, 1, &dst)) return syscall_fail(tm, CELL_EFAULT);
	if ((int)fd64 == CELL_STDIN_FILENO) { return 0; }
	cell_task_fd_t *f = &tm->fds[(int)fd64];
	size_t got = 0;
	cell_vfs_status_t st = cell_vfs_read_at(vfs, env, f->path, f->offset, dst, count, &got);
	if (st != CELL_VFS_OK) return syscall_fail(tm, vfs_errno(st));
	f->offset += (uint32_t)got;
	return (int64_t)got;
}

static int64_t sys_write(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const cell_exec_t *exec, outbuf_t *out,
	int64_t fd64, uint64_t addr, uint64_t count64) {
	if (fd64 < 0 || fd64 >= (int64_t)CELL_TASK_FD_MAX || !fd_valid(tm, (int)fd64) || !tm->fds[(int)fd64].writable)
		return syscall_fail(tm, CELL_EBADF);
	if (count64 > 0xffffffffu) return syscall_fail(tm, CELL_EINVAL);
	uint32_t count = (uint32_t)count64;
	uint8_t *src = 0;
	if (!pointer_span(exec, tm->memory, addr, count, 0, &src)) return syscall_fail(tm, CELL_EFAULT);
	if ((int)fd64 == CELL_STDOUT_FILENO || (int)fd64 == CELL_STDERR_FILENO) {
		if (!output_bytes(out, src, count)) return syscall_fail(tm, CELL_EIO);
		return count;
	}
	cell_task_fd_t *f = &tm->fds[(int)fd64];
	if (f->append) {
		char absolute[CELL_VFS_PATH_MAX]; size_t size = 0;
		cell_vfs_status_t ost = cell_vfs_open_file(vfs, env, f->path, CELL_VFS_OPEN_WRITE, absolute, &size);
		if (ost != CELL_VFS_OK) return syscall_fail(tm, vfs_errno(ost));
		f->offset = (uint32_t)size;
	}
	size_t size = 0;
	cell_vfs_status_t st = cell_vfs_write_at(vfs, f->path, f->offset, src, count, &size);
	if (st != CELL_VFS_OK) return syscall_fail(tm, vfs_errno(st));
	f->offset += count;
	return count;
}

static int64_t sys_lseek(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, int64_t fd64, int64_t off, int64_t whence) {
	if (fd64 < 0 || fd64 >= (int64_t)CELL_TASK_FD_MAX || !fd_valid(tm, (int)fd64))
		return syscall_fail(tm, CELL_EBADF);
	if (fd64 <= CELL_STDERR_FILENO) return syscall_fail(tm, CELL_ESPIPE);
	cell_task_fd_t *f = &tm->fds[(int)fd64];
	int64_t base = 0;
	if (whence == CELL_SEEK_SET) base = 0;
	else if (whence == CELL_SEEK_CUR) base = f->offset;
	else if (whence == CELL_SEEK_END) {
		char absolute[CELL_VFS_PATH_MAX]; size_t size = 0;
		uint32_t mode = f->writable ? CELL_VFS_OPEN_WRITE : CELL_VFS_OPEN_READ;
		cell_vfs_status_t st = cell_vfs_open_file(vfs, env, f->path, mode, absolute, &size);
		if (st != CELL_VFS_OK) return syscall_fail(tm, vfs_errno(st));
		base = (int64_t)size;
	} else return syscall_fail(tm, CELL_EINVAL);
	int64_t pos = base + off;
	if (pos < 0 || pos > (int64_t)CELLFS_FILE_MAX) return syscall_fail(tm, CELL_EINVAL);
	f->offset = (uint32_t)pos;
	return pos;
}

static int64_t sys_malloc(cell_task_manager_t *tm, const cell_exec_t *exec, uint64_t size64) {
	if (!size64) { return 0; }
	if (size64 > CELL_EXEC_MEMORY_MAX) return syscall_fail(tm, CELL_ENOMEM);
	uint32_t size = ((uint32_t)size64 + 7u) & ~7u;
	for (uint32_t i = 0; i < CELL_TASK_HEAP_BLOCKS; ++i) {
		cell_task_heap_block_t *b = &tm->heap[i];
		if (!b->used && b->size >= size && b->offset) {
			b->used = 1u;
			return b->offset;
		}
	}
	uint32_t slot = CELL_TASK_HEAP_BLOCKS;
	for (uint32_t i = 0; i < CELL_TASK_HEAP_BLOCKS; ++i) if (!tm->heap[i].offset) { slot = i; break; }
	if (slot == CELL_TASK_HEAP_BLOCKS || tm->heap_next > exec->h->memory_bytes || size > exec->h->memory_bytes - tm->heap_next)
		return syscall_fail(tm, CELL_ENOMEM);
	cell_task_heap_block_t *b = &tm->heap[slot];
	b->offset = tm->heap_next; b->size = size; b->used = 1u;
	tm->heap_next += size;
	return b->offset;
}

static int64_t sys_free(cell_task_manager_t *tm, uint64_t addr) {
	if (!addr) { return 0; }
	for (uint32_t i = 0; i < CELL_TASK_HEAP_BLOCKS; ++i) {
		if (tm->heap[i].used && tm->heap[i].offset == addr) {
			tm->heap[i].used = 0u;
			return 0;
		}
	}
	return syscall_fail(tm, CELL_EINVAL);
}

static int64_t execute_syscall(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const cell_exec_t *exec, outbuf_t *out,
	uint8_t nr, uint64_t a0, uint64_t a1, uint64_t a2) {
	switch (nr) {
	case CELL_EXEC_SYS_OPEN: return sys_open(tm, vfs, env, exec, a0, a1);
	case CELL_EXEC_SYS_CLOSE: return sys_close(tm, (int64_t)a0);
	case CELL_EXEC_SYS_READ: return sys_read(tm, vfs, env, exec, (int64_t)a0, a1, a2);
	case CELL_EXEC_SYS_WRITE: return sys_write(tm, vfs, env, exec, out, (int64_t)a0, a1, a2);
	case CELL_EXEC_SYS_LSEEK: return sys_lseek(tm, vfs, env, (int64_t)a0, (int64_t)a1, (int64_t)a2);
	case CELL_EXEC_SYS_ERRNO: return tm->errno_value;
	case CELL_EXEC_SYS_MALLOC: return sys_malloc(tm, exec, a0);
	case CELL_EXEC_SYS_FREE: return sys_free(tm, a0);
	default: return syscall_fail(tm, CELL_EINVAL);
	}
}

static int run_impl(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const char *path,
	uint32_t argc, const char *const argv[],
	char *out, size_t cap, uint32_t *task_id) {
	if (!tm || !vfs || !env || !path || !*path || !out || cap < 2u) return 0;
	char absolute[CELL_VFS_PATH_MAX];
	if (!cell_vfs_normalize(vfs, path, absolute)) return 0;
	cell_task_record_t *r = new_record(tm, absolute);
	if (task_id) *task_id = r->id;
	outbuf_t b; ob_init(&b, out, cap);
	if (!(absolute[0]=='/' && absolute[1]=='p' && absolute[2]=='r' && absolute[3]=='o' &&
	      absolute[4]=='g' && absolute[5]=='r' && absolute[6]=='a' && absolute[7]=='m' &&
	      absolute[8]=='s' && absolute[9]=='/')) {
		fault(r, CELL_TASK_FAULT_PERMISSION);
		ob_str(&b, absolute); ob_str(&b, ": Permission denied.");
		return b.ok;
	}

	size_t image_bytes = 0;
	cell_vfs_status_t vs = cell_vfs_read_bytes(vfs, absolute, tm->image, sizeof(tm->image), &image_bytes);
	if (vs != CELL_VFS_OK) {
		fault(r, CELL_TASK_FAULT_LOAD);
		ob_str(&b, absolute); ob_str(&b, vs == CELL_VFS_NOT_FOUND ? ": No such file or directory." : ": I/O error.");
		return b.ok;
	}
	cell_exec_t exec;
	cell_exec_status_t es = cell_exec_open(&exec, tm->image, image_bytes, all_known_caps());
	if (es != CELL_EXEC_OK) {
		fault(r, CELL_TASK_FAULT_LOAD);
		ob_str(&b, absolute); ob_str(&b, ": Exec format error.");
		return b.ok;
	}
	r->declared_caps = exec.h->capability_mask;
	if (r->declared_caps & ~tm->policy_mask) {
		fault(r, CELL_TASK_FAULT_PERMISSION);
		ob_str(&b, absolute); ob_str(&b, ": Permission denied.");
		return b.ok;
	}
	r->granted_caps = r->declared_caps;
	zero_bytes(tm->regs, sizeof(tm->regs));
	zero_bytes(tm->memory, sizeof(tm->memory));
	reset_process_state(tm);
	if (!prepare_argv(tm, &exec, argc, argv)) {
		fault(r, CELL_TASK_FAULT_ARGUMENT);
		ob_str(&b, absolute); ob_str(&b, ": Argument list too large.");
		return b.ok;
	}
	r->pc = exec.h->entry_pc;
	r->state = CELL_TASK_RUNNING;

	for (;;) {
		if (r->gas_used >= exec.h->gas_limit) { fault(r, CELL_TASK_FAULT_GAS); break; }
		if (r->pc >= exec.instruction_count) { fault(r, CELL_TASK_FAULT_PC); break; }
		const cell_exec_insn_t *in = &exec.code[r->pc];
		uint32_t next_pc = r->pc + 1u;
		++r->gas_used;
		switch (in->opcode) {
		case CELL_EXEC_OP_HALT:
			r->exit_code = in->imm; r->state = CELL_TASK_EXITED; r->fault = CELL_TASK_FAULT_NONE; break;
		case CELL_EXEC_OP_EXIT:
			r->exit_code = (int32_t)tm->regs[in->a]; r->state = CELL_TASK_EXITED; r->fault = CELL_TASK_FAULT_NONE; break;
		case CELL_EXEC_OP_MOVI: tm->regs[in->dst] = (uint64_t)(int64_t)in->imm; break;
		case CELL_EXEC_OP_MOV: tm->regs[in->dst] = tm->regs[in->a]; break;
		case CELL_EXEC_OP_ADD: tm->regs[in->dst] = tm->regs[in->a] + tm->regs[in->b]; break;
		case CELL_EXEC_OP_ADDI: tm->regs[in->dst] = tm->regs[in->a] + (uint64_t)(int64_t)in->imm; break;
		case CELL_EXEC_OP_SUB: tm->regs[in->dst] = tm->regs[in->a] - tm->regs[in->b]; break;
		case CELL_EXEC_OP_MUL: tm->regs[in->dst] = tm->regs[in->a] * tm->regs[in->b]; break;
		case CELL_EXEC_OP_DIV:
		case CELL_EXEC_OP_MOD: {
			int64_t x = (int64_t)tm->regs[in->a], y = (int64_t)tm->regs[in->b];
			if (!y || (x == (-9223372036854775807ll - 1ll) && y == -1ll)) { fault(r, CELL_TASK_FAULT_ARITHMETIC); break; }
			tm->regs[in->dst] = (uint64_t)(in->opcode == CELL_EXEC_OP_DIV ? x / y : x % y);
			break;
		}
		case CELL_EXEC_OP_CMPEQ: tm->regs[in->dst] = tm->regs[in->a] == tm->regs[in->b]; break;
		case CELL_EXEC_OP_CMPNE: tm->regs[in->dst] = tm->regs[in->a] != tm->regs[in->b]; break;
		case CELL_EXEC_OP_CMPLT: tm->regs[in->dst] = (int64_t)tm->regs[in->a] < (int64_t)tm->regs[in->b]; break;
		case CELL_EXEC_OP_CMPLE: tm->regs[in->dst] = (int64_t)tm->regs[in->a] <= (int64_t)tm->regs[in->b]; break;
		case CELL_EXEC_OP_CMPGT: tm->regs[in->dst] = (int64_t)tm->regs[in->a] > (int64_t)tm->regs[in->b]; break;
		case CELL_EXEC_OP_CMPGE: tm->regs[in->dst] = (int64_t)tm->regs[in->a] >= (int64_t)tm->regs[in->b]; break;
		case CELL_EXEC_OP_OR: tm->regs[in->dst] = tm->regs[in->a] | tm->regs[in->b]; break;
		case CELL_EXEC_OP_JZ: if (tm->regs[in->a] == 0u) next_pc = (uint32_t)((int64_t)r->pc + 1 + in->imm); break;
		case CELL_EXEC_OP_JNZ: if (tm->regs[in->a] != 0u) next_pc = (uint32_t)((int64_t)r->pc + 1 + in->imm); break;
		case CELL_EXEC_OP_JMP: next_pc = (uint32_t)((int64_t)r->pc + 1 + in->imm); break;
		case CELL_EXEC_OP_PUTS: {
			uint32_t len = (uint32_t)in->a | ((uint32_t)in->b << 8);
			if (!output_bytes(&b, exec.data + (uint32_t)in->imm, len)) fault(r, CELL_TASK_FAULT_OUTPUT);
			break;
		}
		case CELL_EXEC_OP_PUTSM:
			if (!output_pointer_string(&b, &exec, tm->memory, tm->regs[in->a], 1)) fault(r, CELL_TASK_FAULT_OUTPUT);
			break;
		case CELL_EXEC_OP_PUTC: {
			uint8_t ch = (uint8_t)tm->regs[in->a];
			if (!output_char_ok(ch)) fault(r, CELL_TASK_FAULT_OUTPUT); else ob_char(&b, (char)ch);
			break;
		}
		case CELL_EXEC_OP_CAP: {
			cell_capability_id_t id = (cell_capability_id_t)in->imm;
			uint64_t bit = 1ull << (uint32_t)id;
			if (!(r->granted_caps & bit)) { fault(r, CELL_TASK_FAULT_PERMISSION); break; }
			if (!append_capability(&b, env, id, &tm->regs[in->dst])) fault(r, CELL_TASK_FAULT_CAPABILITY);
			break;
		}
		case CELL_EXEC_OP_LOAD8: {
			uint64_t addr = tm->regs[in->a] + (uint64_t)(int64_t)in->imm;
			uint8_t *p = 0;
			if (!pointer_span(&exec, tm->memory, addr, 1u, 0, &p)) fault(r, CELL_TASK_FAULT_MEMORY);
			else tm->regs[in->dst] = *p;
			break;
		}
		case CELL_EXEC_OP_LOAD64: {
			int64_t addr = (int64_t)tm->regs[in->a] + (int64_t)in->imm;
			if (addr < 0 || (uint64_t)addr + 8u > exec.h->memory_bytes) fault(r, CELL_TASK_FAULT_MEMORY);
			else tm->regs[in->dst] = load_u64(tm->memory, (uint32_t)addr);
			break;
		}
		case CELL_EXEC_OP_STORE8: {
			int64_t addr = (int64_t)tm->regs[in->a] + (int64_t)in->imm;
			if (addr < 0 || (uint64_t)addr >= exec.h->memory_bytes) fault(r, CELL_TASK_FAULT_MEMORY);
			else tm->memory[(uint32_t)addr] = (uint8_t)tm->regs[in->b];
			break;
		}
		case CELL_EXEC_OP_STRLEN: {
			uint32_t n = 0;
			if (!pointer_cstr(&exec, tm->memory, tm->regs[in->a], &n, 0)) fault(r, CELL_TASK_FAULT_MEMORY);
			else tm->regs[in->dst] = n;
			break;
		}
		case CELL_EXEC_OP_ATOI: {
			int64_t value = 0;
			if (!atoi_memory(&exec, tm->memory, tm->regs[in->a], &value)) fault(r, CELL_TASK_FAULT_MEMORY);
			else tm->regs[in->dst] = (uint64_t)value;
			break;
		}
		case CELL_EXEC_OP_STRCMPC: {
			int64_t value = 0;
			if (!strcmp_memory_data(&exec, tm->memory, tm->regs[in->a], (uint32_t)in->imm, &value)) fault(r, CELL_TASK_FAULT_MEMORY);
			else tm->regs[in->dst] = (uint64_t)value;
			break;
		}
		case CELL_EXEC_OP_SYSCALL: {
			uint8_t nr = CELL_EXEC_SYSCALL_NR(in->imm);
			uint8_t arg2 = CELL_EXEC_SYSCALL_ARG2(in->imm);
			int64_t result = execute_syscall(tm, vfs, env, &exec, &b, nr,
				tm->regs[in->a], tm->regs[in->b], tm->regs[arg2]);
			tm->regs[in->dst] = (uint64_t)result;
			break;
		}
		case CELL_EXEC_OP_CALL: {
			if (tm->call_depth >= CELL_TASK_CALL_DEPTH) { fault(r, CELL_TASK_FAULT_STACK); break; }
			uint8_t argc_call = CELL_EXEC_CALL_ARGC(in->imm);
			uint64_t arg0 = argc_call >= 1u ? tm->regs[in->a] : 0u;
			uint64_t arg1 = argc_call >= 2u ? tm->regs[in->b] : 0u;
			cell_task_call_frame_t *frame = &tm->calls[tm->call_depth++];
			frame->return_pc = next_pc; frame->dst = in->dst;
			for (uint32_t i = 0; i < CELL_EXEC_REGS; ++i) frame->regs[i] = tm->regs[i];
			zero_bytes(tm->regs, sizeof(tm->regs));
			if (argc_call >= 1u) tm->regs[0] = arg0;
			if (argc_call >= 2u) tm->regs[1] = arg1;
			next_pc = CELL_EXEC_CALL_TARGET(in->imm);
			break;
		}
		case CELL_EXEC_OP_RET: {
			if (!tm->call_depth) { fault(r, CELL_TASK_FAULT_STACK); break; }
			uint64_t result = tm->regs[in->a];
			cell_task_call_frame_t *frame = &tm->calls[--tm->call_depth];
			for (uint32_t i = 0; i < CELL_EXEC_REGS; ++i) tm->regs[i] = frame->regs[i];
			tm->regs[frame->dst] = result;
			next_pc = frame->return_pc;
			break;
		}
		default: fault(r, CELL_TASK_FAULT_INSTRUCTION); break;
		}
		if (r->state == CELL_TASK_EXITED || r->state == CELL_TASK_FAULTED) break;
		r->pc = next_pc;
	}

	if (r->state == CELL_TASK_FAULTED) {
		if (b.p != out) ob_char(&b, '\n');
		ob_str(&b, absolute); ob_str(&b, ": runtime fault: ");
		ob_str(&b, cell_task_fault_name(r->fault)); ob_char(&b, '.');
	}
	return b.ok;
}

int cell_task_run(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const char *path,
	char *out, size_t cap, uint32_t *task_id) {
	return run_impl(tm, vfs, env, path, 0, 0, out, cap, task_id);
}

int cell_task_run_argv(cell_task_manager_t *tm, cell_vfs_t *vfs,
	const cell_capability_env_t *env, const char *path,
	uint32_t argc, const char *const argv[],
	char *out, size_t cap, uint32_t *task_id) {
	return run_impl(tm, vfs, env, path, argc, argv, out, cap, task_id);
}

static const cell_task_record_t *find_task(const cell_task_manager_t *tm, uint32_t id) {
	if (!tm || !id) return 0;
	for (uint32_t i = 0; i < CELL_TASK_HISTORY; ++i)
		if (tm->history[i].state != CELL_TASK_EMPTY && tm->history[i].id == id) return &tm->history[i];
	return 0;
}

static void describe_record(outbuf_t *b, const cell_task_record_t *r) {
	ob_u32(b, r->id); ob_char(b, ' '); ob_str(b, cell_task_state_name(r->state)); ob_char(b, ' ');
	if (r->state == CELL_TASK_EXITED) ob_i32(b, r->exit_code); else ob_char(b, '-');
	ob_char(b, ' '); ob_u32(b, r->gas_used); ob_char(b, ' '); ob_str(b, r->path);
	if (r->state == CELL_TASK_FAULTED) { ob_str(b, " ["); ob_str(b, cell_task_fault_name(r->fault)); ob_char(b, ']'); }
}

static void ps_header(outbuf_t *b) { ob_str(b, "PID STATE EXIT GAS COMMAND"); }

int cell_task_list(const cell_task_manager_t *tm, char *out, size_t cap) {
	if (!tm || !out || !cap) return 0;
	outbuf_t b; ob_init(&b, out, cap); ps_header(&b);
	uint32_t min_id = tm->next_id > CELL_TASK_HISTORY ? tm->next_id - CELL_TASK_HISTORY : 1u;
	for (uint32_t id = min_id; id < tm->next_id; ++id) {
		const cell_task_record_t *r = find_task(tm, id);
		if (!r) continue;
		ob_char(&b, '\n'); describe_record(&b, r);
	}
	return b.ok;
}

int cell_task_describe(const cell_task_manager_t *tm, uint32_t id, char *out, size_t cap) {
	if (!tm || !out || !cap) return 0;
	const cell_task_record_t *r = find_task(tm, id);
	if (!r) { copy_text(out, cap, "ps: process not found."); return 1; }
	outbuf_t b; ob_init(&b, out, cap); ps_header(&b); ob_char(&b, '\n'); describe_record(&b, r); return b.ok;
}
