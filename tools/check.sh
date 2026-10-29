#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# The checks of this repository (README.md, "Checks"):
#   1. host tests of the device-independent code, with ASan and UBSan;
#   2. the screen previews in docs/img (--previews rewrites them);
#   3. a ColdFire cross-compile of each manifest with elekloader's flags for the device: no undefined symbol may be
#      left (the mods have no C library), and the sizes are printed;
#   4. with DT2_SYX / DN2_SYX set to the stock OS files, elekloader's own build and lint of each mod, and the OS file
#      out/check/macro-<dev>.syx (never commit it: it holds Elektron's firmware).
# Each device's manifest is mods/<dev>/mod.json plus its features' fragments (tools/manifest.py): kits.json unless
# MM_KITS=0. MM_LED_TEST=1 builds the LED colour test. Each built manifest's storage is checked (tools/storage.py),
# against the other mods MM_WITH names (mod.json or .elemod files, space-separated) too.
# It runs on the bash macOS ships (3.2) as well as on Linux. The ColdFire toolchain is m68k-linux-gnu- (Debian,
# Ubuntu) or m68k-elf- (macOS, bare-metal builds), found on the PATH; ELEKLOADER_CROSS names another prefix.
set -euo pipefail

# the stock OS files may be given relative to where the script is run from
abspath() { [[ -z "$1" || "$1" == /* ]] && echo "$1" || echo "$PWD/$1"; }
DT2_SYX=$(abspath "${DT2_SYX:-}")
DN2_SYX=$(abspath "${DN2_SYX:-}")
WITH=()   # MM_WITH's mods, from where the script is run
for o in ${MM_WITH:-}; do WITH+=(--with "$(abspath "$o")"); done
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
# manifest DEV EXPR: a Python expression over the device's built manifest m (tools/manifest.py), one line per item
manifest() { python3 -c "import json; m = json.load(open('$OUT/$1-manifest/mod.json')); print('\n'.join($2))"; }
SRC=(src/mm_logic.c src/mm_apply.c src/mm_store.c src/mm_draw.c src/mm_scope.c)
FEATURES=()
[[ "${MM_KITS:-1}" == 0 ]] || FEATURES+=(kits)
EXTRA=()
[[ -z "${MM_LED_TEST:-}" ]] || EXTRA+=(--cflag=-DMM_LED_TEST)

echo "== host tests"
"$CC" -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
    -o "$OUT/test_mm" "${SRC[@]}" tests/test_mm.c tests/test_draw.c
"$OUT/test_mm"
python3 tests/test_storage.py 2>&1 | tail -1

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
    python3 tools/manifest.py "$dev" "$OUT/$dev-manifest" ${FEATURES[@]+"${FEATURES[@]}"} ${EXTRA[@]+"${EXTRA[@]}"}
    lines cflags manifest "$dev" "m['cflags']"
    lines srcs manifest "$dev" "m['sources']"
    lines defsym manifest "$dev" "'--defsym=%s=%s' % kv for kv in m.get('defsym', {}).items()"
    lines tables manifest "$dev" "m.get('collections', {})"
    objs=()
    for s in "${srcs[@]}"; do
        o="$OUT/$dev-$(basename "${s%.*}").o"
        if [[ "$s" == *.s ]]; then
            "${CROSS}as" -mcpu=54455 ${defsym[@]+"${defsym[@]}"} "$OUT/$dev-manifest/$s" -o "$o"
        else
            "${CROSS}gcc" "${FLAGS[@]}" ${cflags[@]+"${cflags[@]}"} -c "$OUT/$dev-manifest/$s" -o "$o"
        fi
        objs+=("$o")
    done
    "${CROSS}ld" -r -o "$OUT/$dev.o" "${objs[@]}"
    # the collections the mod declares are the linker's tables; core_additem is the core's (PERSONALIZE's row)
    undef=$("${CROSS}nm" -u "$OUT/$dev.o" | awk '{print $2}' |
        grep -v -x -F -f <(printf '%s\n' ${tables[@]+"${tables[@]}"} core_additem "") || true)
    if [[ -n "$undef" ]]; then
        echo "$dev: undefined symbols (the mod has no C library):"; echo "$undef"; exit 1
    fi
    for fn in mm_ev_key mm_ev_enc mm_ev_tick mm_ev_draw; do
        "${CROSS}nm" "$OUT/$dev.o" | grep -q " T $fn$" || { echo "$dev: $fn missing"; exit 1; }
    done
    echo "$dev: ok, $("${CROSS}size" "$OUT/$dev.o" | tail -1 | awk '{print "text " $1 ", data " $2 ", bss " $3}')"
    # the storage it claims, against what the build writes and against MM_WITH's mods
    "${CROSS}gcc" "${FLAGS[@]}" -c tests/emu/layout.c -o "$OUT/$dev-layout.o"
    setup_size=$((16#$("${CROSS}nm" "$OUT/$dev-layout.o" | awk '$3 == "MMOFF_setup_size" {print $1}')))
    python3 tools/storage.py "$OUT/$dev-manifest" --setup-size "$setup_size" ${WITH[@]+"${WITH[@]}"}
done

if [[ -n "$DT2_SYX$DN2_SYX" ]]; then
    echo "== elekloader build and lint"
    EL=third_party/elekloader
    [[ -f "$EL/elekloader/__init__.py" ]] || {
        echo "$EL is empty: fetch it with  git submodule update --init" >&2
        exit 1
    }
    export ELEKLOADER_CROSS=$CROSS
    # elekloader from the submodule, run from this directory: python3 with it on the module path
    el() { PYTHONPATH="$EL${PYTHONPATH:+:$PYTHONPATH}" python3 "$@"; }
    for pair in "dt2:$DT2_SYX:core-dt2" "dn2:$DN2_SYX:core-dn2"; do
        IFS=: read -r dev syx core <<< "$pair"
        [[ -n "$syx" ]] || continue
        el -m elekloader.sdk.build "$EL/mods/$core" --stock "$syx" --out "$OUT/$dev-core"
        el -m elekloader.sdk.build "$OUT/$dev-manifest" --stock "$syx" --out "$OUT/$dev-mod"
        el -m elekloader.lint "$OUT/$dev-mod"/*.elemod --stock "$syx" --with "$OUT/$dev-core"/*.elemod
        el -m elekloader.patch --stock "$syx" --mod "$OUT/$dev-core"/*.elemod --mod "$OUT/$dev-mod"/*.elemod \
            --out "$OUT/macro-$dev.syx" --version MMDV | tail -2
        # where the mod's symbols landed, for the emulator scenarios (the layout object is the cross-compile's)
        el - "$syx" "$OUT/$dev-core"/*.elemod "$OUT/$dev-mod"/*.elemod "$OUT/macro-$dev.map.json" \
            "$OUT/$dev-layout.o" "${CROSS}nm" <<'PY'
import json, subprocess, sys
from elekloader import patch
stock, core, mod, out, layout, nm = sys.argv[1:7]
_, manifest = patch.build(stock, [core, mod], 'MMDV', log=lambda *a: None)
secs = manifest['link']['layout']['sections']
m = json.load(open(mod))
syms = {k: secs['macro-mode ' + v[0]] + v[1] for k, v in m['symbols'].items() if v[0] in ('.run', '.bss')}
syms['layout'] = {l.split()[2][6:]: int(l.split()[0], 16)
                  for l in subprocess.run([nm, layout], capture_output=True, text=True).stdout.splitlines()
                  if ' A MMOFF_' in l}
json.dump(syms, open(out, 'w'), indent=1, sort_keys=True)
PY
    done
else
    echo "== elekloader build skipped: set DT2_SYX and/or DN2_SYX to the stock OS files"
fi
