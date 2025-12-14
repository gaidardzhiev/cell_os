#!/usr/bin/env bash
# Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
# SPDX-License-Identifier: MIT
#
# This file is licensed under the MIT License.
# See the LICENSE file in the project root for full license text.

set -euo pipefail

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <qemu-log> [<qemu-log>...]" >&2
	exit 1
fi

required_markers=(
	"#E1 handoff"
	"#E3/E4 CG proof:"
	"#E5 ORG proof:"
	"#E6 EVT proof:"
	"#E7 UPD proof:"
	"#E8 OBS proof:"
	"#E9 SEC proof:"
	"#E10 RELEASE proof:"
	"FULL PROOF SUCCESS"
)

status=0
for log in "$@"; do
	if [[ ! -s "$log" ]]; then
		echo "Missing or empty log: ${log}" >&2
		status=1
		continue
	fi
	for marker in "${required_markers[@]}"; do
		if ! grep -Fq "$marker" "$log"; then
			echo "Missing marker '${marker}' in ${log}" >&2
			status=1
		fi
	done
done

if [[ $status -eq 0 ]]; then
	echo "# proofs ok ($# log(s))"
fi

exit $status
