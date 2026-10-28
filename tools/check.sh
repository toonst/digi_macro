#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# The checks of this repository (README.md, "Checks"):
#   1. host tests of the device-independent code, with ASan and UBSan;
#   2. the screen previews in docs/img (--previews rewrites them);
#   3. a ColdFire cross-compile of each manifest with elekloader's flags for the device: no undefined symbol may be
#      left (the mods have no C library), and the sizes are printed;
#   4. with DT2_SYX / DN2_SYX set to the stock OS files, elekloader's own build and lint of each mod, and the OS file
#      out/check/macro-<dev>.syx (never commit it: it holds Elektron's firmware).
# It runs on the bash macOS ships (3.2) as well as on Linux. The ColdFire toolchain is m68k-linux-gnu- (Debian,
# Ubuntu) or m68k-elf- (macOS, bare-metal builds), found on the PATH; ELEKLOADER_CROSS names another prefix.
set -euo pipefail

# the stock OS files may be given relative to where the script is run from
abspath() { [[ -z "$1" || "$1" == /* ]] && echo "$1" || echo "$PWD/$1"; }
DT2_SYX=$(abspath "${DT2_SYX:-}")
DN2_SYX=$(abspath "${DN2_SYX:-}")
for f in "$DT2_SYX" "$DN2_SYX"; do
    [[ -z "$f" || -f "$f" ]] || { echo "no such file: $f" >&2; exit 1; }
done

cd "$(dirname "$0")/.."
OUT=${OUT:-out/check}
CC=${CC:-cc}
if [[ -z "${ELEKLOADER_CROSS:-}" ]]; then
    for p in m68k-linux-gnu- m68k-elf-; do
        if command -v "${p}gcc" >/dev/null; then ELEKLOADER_CROSS=$p; break; fi
    done
fi
CROSS=${ELEKLOADER_CROSS:-m68k-linux-gnu-}
mkdir -p "$OUT"

# lines ARRAY CMD...: the output lines of CMD into ARRAY (mapfile, which bash 3.2 lacks)
lines() {
    local name=$1 line
    shift
    eval "$name=()"
    while IFS= read -r line; do
        if [[ -n "$line" ]]; then eval "$name+=(\"\$line\")"; fi
    done < <("$@")
}
# manifest DEV EXPR: a Python expression over the manifest m, one line per item
manifest() { python3 -c "import json; m = json.load(open('mods/$1/mod.json')); print('\n'.join($2))"; }
SRC=(src/mm_logic.c src/mm_apply.c src/mm_store.c src/mm_draw.c)

echo "== host tests"
"$CC" -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
    -o "$OUT/test_mm" "${SRC[@]}" tests/test_mm.c tests/test_draw.c
"$OUT/test_mm"

echo "== previews"
"$CC" -std=c99 -Wall -Wextra -Werror -O1 -o "$OUT/preview" "${SRC[@]}" tests/preview.c
"$OUT/preview" "$OUT"
if [[ "${1:-}" == "--previews" ]]; then
    python3 tools/pbm2png.py docs/img "$OUT"/*.pbm
    echo "docs/img rewritten"
fi

echo "== ColdFire cross-compile"
command -v "${CROSS}gcc" >/dev/null || {
    echo "no ColdFire compiler (${CROSS}gcc): install m68k-linux-gnu or m68k-elf binutils and gcc, or set ELEKLOADER_CROSS" >&2
    exit 1
}
FLAGS=(-mcpu=54455 -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -fomit-frame-pointer -Wall -Wextra -Werror)
for dev in dt2 dn2; do
    lines cflags manifest "$dev" "m['cflags']"
    lines srcs manifest "$dev" "m['sources']"
    lines defsym manifest "$dev" "'--defsym=%s=%s' % kv for kv in m.get('defsym', {}).items()"
    lines tables manifest "$dev" "m.get('collections', {})"
    objs=()
    for s in "${srcs[@]}"; do
        o="$OUT/$dev-$(basename "${s%.*}").o"
        if [[ "$s" == *.s ]]; then
            "${CROSS}as" -mcpu=54455 ${defsym[@]+"${defsym[@]}"} "mods/$dev/$s" -o "$o"
        else
            "${CROSS}gcc" "${FLAGS[@]}" ${cflags[@]+"${cflags[@]}"} -c "mods/$dev/$s" -o "$o"
        fi
        objs+=("$o")
    done
    "${CROSS}ld" -r -o "$OUT/$dev.o" "${objs[@]}"
    # the collections the mod declares are the linker's tables
    undef=$("${CROSS}nm" -u "$OUT/$dev.o" | awk '{print $2}' | grep -v -x -F -f <(printf '%s\n' ${tables[@]+"${tables[@]}"} "") || true)
    if [[ -n "$undef" ]]; then
        echo "$dev: undefined symbols (the mod has no C library):"; echo "$undef"; exit 1
    fi
    for fn in mm_ev_key mm_ev_enc mm_ev_tick mm_ev_draw; do
        "${CROSS}nm" "$OUT/$dev.o" | grep -q " T $fn$" || { echo "$dev: $fn missing"; exit 1; }
    done
    echo "$dev: ok, $("${CROSS}size" "$OUT/$dev.o" | tail -1 | awk '{print "text " $1 ", data " $2 ", bss " $3}')"
done

if [[ -n "$DT2_SYX$DN2_SYX" ]]; then
    echo "== elekloader build and lint"
    EL=third_party/elekloader
    [[ -f "$EL/elekloader/__init__.py" ]] || {
        echo "$EL is empty: fetch it with  git submodule update --init" >&2
        exit 1
    }
    export ELEKLOADER_CROSS=$CROSS
    for pair in "dt2:$DT2_SYX:core-dt2" "dn2:$DN2_SYX:core-dn2"; do
        IFS=: read -r dev syx core <<< "$pair"
        [[ -n "$syx" ]] || continue
        (cd "$EL" && python3 -m elekloader.sdk.build "mods/$core" --stock "$syx" --out "$OLDPWD/$OUT/$dev-core")
        (cd "$EL" && python3 -m elekloader.sdk.build "$OLDPWD/mods/$dev" --stock "$syx" --out "$OLDPWD/$OUT/$dev-mod")
        (cd "$EL" && python3 -m elekloader.lint "$OLDPWD/$OUT/$dev-mod"/*.elemod --stock "$syx" --with "$OLDPWD/$OUT/$dev-core"/*.elemod)
        (cd "$EL" && python3 -m elekloader.patch --stock "$syx" --mod "$OLDPWD/$OUT/$dev-core"/*.elemod \
            --mod "$OLDPWD/$OUT/$dev-mod"/*.elemod --out "$OLDPWD/$OUT/macro-$dev.syx" --version MMDV | tail -2)
        # where the mod's symbols landed, for the emulator scenarios
        (cd "$EL" && python3 - "$syx" "$OLDPWD/$OUT/$dev-core"/*.elemod "$OLDPWD/$OUT/$dev-mod"/*.elemod "$OLDPWD/$OUT/macro-$dev.map.json" <<'PY'
import json, sys
from elekloader import patch
stock, core, mod, out = sys.argv[1:5]
_, manifest = patch.build(stock, [core, mod], 'MMDV', log=lambda *a: None)
secs = manifest['link']['layout']['sections']
m = json.load(open(mod))
syms = {k: secs['macro-mode ' + v[0]] + v[1] for k, v in m['symbols'].items() if v[0] in ('.run', '.bss')}
json.dump(syms, open(out, 'w'), indent=1, sort_keys=True)
PY
        )
    done
else
    echo "== elekloader build skipped: set DT2_SYX and/or DN2_SYX to the stock OS files"
fi
