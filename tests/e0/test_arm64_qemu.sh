#!/usr/bin/env bash
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: GPL-3.0-or-later
#

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

TMPDIR="${TMPDIR:-/tmp}" make -C "$ROOT" run-e0-arm

LOG="${TMPDIR}/arm.log"

grep -q "arch=arm64" "$LOG"
grep -q "tier=QEMU_VIRT" "$LOG"
grep -q "intr=GICV2" "$LOG"
grep -q "timer=ARCH_TIMER" "$LOG"
