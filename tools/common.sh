# SPDX-License-Identifier: GPL-2.0-or-later
# What tools/check.sh and tools/build.sh share: the folders, the ColdFire toolchain, the manifests and the
# cross-compile of the mod. Sourced, from the repository's root. It runs on the bash macOS ships (3.2) as well as on
# Linux.
#
#   out/            the firmware files a build makes: macro-dt2.syx, macro-dn2.syx
#   out/build/      everything else: manifests, objects, .elemod files, symbol maps, test binaries, previews
#
# Options, as environment variables: MM_KITS=0 leaves the kits feature out, MM_LED_TEST=1 builds the LED colour test,
# MM_WITH names other mods (mod.json or .elemod files, space-separated) to check the storage claims against,
# ELEKLOADER_CROSS names the ColdFire toolchain's prefix (default: m68k-linux-gnu- or m68k-elf-, whichever is found).

OUT=out
BUILD=out/build
CC=${CC:-cc}
EL=third_party/elekloader
mkdir -p "$BUILD"

# a path given relative to where the script was run from
abspath() { [[ -z "$1" || "$1" == /* ]] && echo "$1" || echo "$CALLER/$1"; }

if [[ -z "${ELEKLOADER_CROSS:-}" ]]; then
    for p in m68k-linux-gnu- m68k-elf-; do
        if command -v "${p}gcc" >/dev/null; then ELEKLOADER_CROSS=$p; break; fi
    done
fi
CROSS=${ELEKLOADER_CROSS:-m68k-linux-gnu-}
export ELEKLOADER_CROSS=$CROSS

WITH=()        # MM_WITH's mods
LINT_WITH=()   # and its .elemod files, for elekloader's lint
for o in ${MM_WITH:-}; do
    WITH+=(--with "$(abspath "$o")")
    [[ "$o" != *.elemod ]] || LINT_WITH+=(--with "$(abspath "$o")")
done

SRC=(src/mm_logic.c src/mm_apply.c src/mm_store.c src/mm_draw.c src/mm_scope.c)
FEATURES=()
[[ "${MM_KITS:-1}" == 0 ]] || FEATURES+=(kits)
EXTRA=()
[[ -z "${MM_LED_TEST:-}" ]] || EXTRA+=(--cflag=-DMM_LED_TEST)

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
manifest() { python3 -c "import json; m = json.load(open('$BUILD/$1-manifest/mod.json')); print('\n'.join($2))"; }

# elekloader from the submodule: python3 with it on the module path
el() { PYTHONPATH="$EL${PYTHONPATH:+:$PYTHONPATH}" python3 "$@"; }

need_cross() {
    command -v "${CROSS}gcc" >/dev/null || {
        echo "no ColdFire compiler (${CROSS}gcc): install m68k-linux-gnu or m68k-elf binutils and gcc (README.md," \
            "\"Installing\"), or set ELEKLOADER_CROSS" >&2
        exit 1
    }
}

need_elekloader() {
    [[ -f "$EL/elekloader/__init__.py" ]] || {
        echo "$EL is empty: fetch it with  git submodule update --init" >&2
        exit 1
    }
}

FLAGS=(-mcpu=54455 -I "$EL/elekloader/sdk/include" -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie
       -fomit-frame-pointer -Wall -Wextra -Werror)

# compile DEV: the device's manifest (mods/DEV/mod.json and the features), cross-compiled with elekloader's flags. No
# undefined symbol may be left (the mod has no C library), and the storage it claims is checked against what the code
# writes and against MM_WITH's mods (tools/storage.py).
compile() {
    local dev=$1 s o undef fn
    local cflags srcs defsym tables objs
    python3 tools/manifest.py "$dev" "$BUILD/$dev-manifest" ${FEATURES[@]+"${FEATURES[@]}"} ${EXTRA[@]+"${EXTRA[@]}"}
    lines cflags manifest "$dev" "m['cflags']"
    lines srcs manifest "$dev" "m['sources']"
    lines defsym manifest "$dev" "'--defsym=%s=%s' % kv for kv in m.get('defsym', {}).items()"
    lines tables manifest "$dev" "m.get('collections', {})"
    objs=()
    for s in "${srcs[@]}"; do
        o="$BUILD/$dev-$(basename "${s%.*}").o"
        if [[ "$s" == *.s ]]; then
            "${CROSS}as" -mcpu=54455 ${defsym[@]+"${defsym[@]}"} "$BUILD/$dev-manifest/$s" -o "$o"
        else
            "${CROSS}gcc" "${FLAGS[@]}" ${cflags[@]+"${cflags[@]}"} -c "$BUILD/$dev-manifest/$s" -o "$o"
        fi
        objs+=("$o")
    done
    "${CROSS}ld" -r -o "$BUILD/$dev.o" "${objs[@]}"
    # the collections the mod declares are the linker's tables; core_* are the core's (mk2/core.h)
    undef=$("${CROSS}nm" -u "$BUILD/$dev.o" | awk '{print $2}' |
        grep -v -x -F -f <(printf '%s\n' ${tables[@]+"${tables[@]}"} core_addvalue core_kit_on \
            core_store_conflicts "") || true)
    if [[ -n "$undef" ]]; then
        echo "$dev: undefined symbols (the mod has no C library):"; echo "$undef"; exit 1
    fi
    for fn in mm_ev_key mm_ev_enc mm_ev_tick mm_ev_draw; do
        "${CROSS}nm" "$BUILD/$dev.o" | grep -q " T $fn$" || { echo "$dev: $fn missing"; exit 1; }
    done
    echo "$dev: compiled, $("${CROSS}size" "$BUILD/$dev.o" | tail -1 | awk '{print "text " $1 ", data " $2 ", bss " $3}')"
    "${CROSS}gcc" "${FLAGS[@]}" -c tests/emu/layout.c -o "$BUILD/$dev-layout.o"
    python3 tools/storage.py "$BUILD/$dev-manifest" ${WITH[@]+"${WITH[@]}"}
}
