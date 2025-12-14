#!/usr/bin/env bash
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

TMPDIR="${TMPDIR:-/tmp}" make -C "$ROOT" run-e0-x86

LOG="${TMPDIR}/x86.log"

grep -q "arch=x86_64" "$LOG"
grep -q "tier=PC_BIOS" "$LOG"
grep -q "intr=PIC_IOAPIC" "$LOG"
grep -q "timer=PIT" "$LOG"
