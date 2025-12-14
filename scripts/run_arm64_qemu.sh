#!/usr/bin/env bash
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ELF="${1:-$ROOT/build/arm64_kernel.elf}"

exec qemu-system-aarch64 \
	-M virt,gic-version=2,secure=off -cpu cortex-a57 -m 64M \
	-nographic -monitor none \
	-serial stdio -no-reboot -no-shutdown \
	-kernel "${ELF}"
