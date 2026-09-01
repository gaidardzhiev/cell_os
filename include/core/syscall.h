/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#define CELL_STDIN_FILENO 0
#define CELL_STDOUT_FILENO 1
#define CELL_STDERR_FILENO 2

#define CELL_O_RDONLY 0x0000
#define CELL_O_WRONLY 0x0001
#define CELL_O_RDWR 0x0002
#define CELL_O_ACCMODE 0x0003
#define CELL_O_CREAT 0x0040
#define CELL_O_TRUNC 0x0200
#define CELL_O_APPEND 0x0400

#define CELL_SEEK_SET 0
#define CELL_SEEK_CUR 1
#define CELL_SEEK_END 2

#define CELL_EPERM 1
#define CELL_ENOENT 2
#define CELL_EIO 5
#define CELL_EBADF 9
#define CELL_ENOMEM 12
#define CELL_EACCES 13
#define CELL_EFAULT 14
#define CELL_EEXIST 17
#define CELL_ENOTDIR 20
#define CELL_EISDIR 21
#define CELL_EINVAL 22
#define CELL_EMFILE 24
#define CELL_EFBIG 27
#define CELL_ENOSPC 28
#define CELL_ESPIPE 29
#define CELL_EROFS 30
#define CELL_ENOTEMPTY 39
