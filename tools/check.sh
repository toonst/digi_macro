#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# The checks of this repository (docs/DEVELOPING.md, "Checks"); it makes no firmware file (that is tools/build.sh):
#   1. host tests of the device-independent code, with ASan and UBSan, and of the storage checker;
#   2. the screen previews (--previews rewrites docs/img);
#   3. a ColdFire cross-compile of each device's manifest, its storage claims checked (tools/common.sh).
#
#   tools/check.sh [--previews]
set -euo pipefail
CALLER=$PWD
cd "$(dirname "$0")/.."
source tools/common.sh

echo "== host tests"
"$CC" -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
    -o "$BUILD/test_mm" "${SRC[@]}" tests/test_mm.c tests/test_draw.c
"$BUILD/test_mm"
python3 tests/test_storage.py 2>&1 | tail -1

echo "== previews"
"$CC" -std=c99 -Wall -Wextra -Werror -O1 -o "$BUILD/preview" "${SRC[@]}" tests/preview.c
"$BUILD/preview" "$BUILD"
if [[ "${1:-}" == "--previews" ]]; then
    python3 tools/pbm2png.py docs/img "$BUILD"/*.pbm
    echo "docs/img rewritten"
fi

echo "== ColdFire cross-compile"
need_cross
for dev in dt2 dn2; do
    compile "$dev"
done
echo "== all checks passed"
