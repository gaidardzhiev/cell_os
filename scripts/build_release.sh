#!/usr/bin/env bash
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

set -euo pipefail
: "${LC_ALL:=C}"
: "${TZ:=UTC}"
: "${SOURCE_DATE_EPOCH:=1700000000}"
export LC_ALL TZ SOURCE_DATE_EPOCH

echo "[release] LC_ALL=$LC_ALL TZ=$TZ SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/../out"
PHO="$OUT/phenos"
mkdir -p "$OUT" "$PHO"

make -C "$ROOT" clean
make -C "$ROOT" all phenotypes
make -C "$ROOT" kernel kernel-arm
mkdir -p "$ROOT/build/tmp"
make -C "$ROOT" test-all >/dev/null
make -C "$ROOT" pack-disk

if [ -f "$ROOT/build/disk.img" ]; then
	cp -f "$ROOT/build/disk.img" "$OUT/cellos-x86_bios.img"
else
	echo "ERROR: missing build/disk.img (run make pack-disk)"; exit 1
fi

if [ -f "$ROOT/build/arm64_kernel.elf" ]; then
	cp -f "$ROOT/build/arm64_kernel.elf" "$OUT/cellos-arm64.elf"
else
	echo "ERROR: missing build/arm64_kernel.elf"; exit 1
fi

cp -f "$ROOT"/build/*.pbin "$PHO"/ 2>/dev/null || true

MAN="$OUT/MANIFEST.txt"
BIN="$OUT/BUILDINFO.json"
: > "$MAN"

for f in "$OUT"/* "$PHO"/*; do
	if [ -f "$f" ]; then
		sz=$(stat -c%s "$f" 2>/dev/null || stat -f%z "$f")
		sha=$(sha256sum "$f" | awk '{print $1}')
		printf "%-40s  %12d  %s\n" "$sha" "$sz" "$(basename "$f")" >> "$MAN"
	fi
done

cat > "$BIN" <<JSON
{
		"version": "$(cat "$ROOT/VERSION")",
		"source_date_epoch": ${SOURCE_DATE_EPOCH},
		"toolchain": {
	"cc": "$(gcc -dumpfullversion -dumpversion 2>/dev/null || true)",
	"ld": "$(ld --version 2>/dev/null | head -1 || true)"
		}
}
JSON

echo "#E10 RELEASE ok"
