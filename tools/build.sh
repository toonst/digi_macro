#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds the firmware file: macro-mode patched into your copy of the official OS (README.md, "Installing").
#
#   tools/build.sh Digitakt_II_OS1.17.syx [Digitone_II_OS1.12.syx]
#
# Each file (a .syx, or Elektron's .zip around it) is known by its checksum: a Digitakt II OS 1.17 gives
# out/macro-dt2.syx, a Digitone II OS 1.12 out/macro-dn2.syx. Those are the files to send to the unit; they hold
# Elektron's firmware, so never commit or share them. Everything else (elekloader's core and the mod as .elemod files,
# the symbol maps the emulator scenarios read) goes to out/build. MM_KITS, MM_LED_TEST and MM_WITH: tools/common.sh.
set -euo pipefail
CALLER=$PWD
cd "$(dirname "$0")/.."
source tools/common.sh

if [[ $# -eq 0 ]]; then
    echo "usage: tools/build.sh path/to/Digitakt_II_OS1.17.syx [path/to/Digitone_II_OS1.12.syx]" >&2
    exit 2
fi
need_cross
need_elekloader

# the device an OS file is for: dt2 or dn2, from elekloader's table of known files
device() {
    el - "$1" <<'PY'
import sys
from elekloader import devices, formats
try:
    _, dev, rel = formats.load(sys.argv[1])
except (OSError, devices.UnknownFirmware, formats.FormatError) as e:
    sys.exit('%s: %s' % (sys.argv[1], e))
want = {'digitakt-mk2': ('dt2', '1.17'), 'digitone-mk2': ('dn2', '1.12')}.get(dev.key)
if not want or want[1] != rel.version:
    sys.exit('%s is %s OS %s: macro-mode needs a Digitakt II OS 1.17 or a Digitone II OS 1.12' % (sys.argv[1], dev.name, rel.version))
print(want[0])
PY
}

built=()
for f in "$@"; do
    syx=$(abspath "$f")
    [[ -f "$syx" ]] || { echo "no such file: $f" >&2; exit 1; }
    dev=$(device "$syx")
    core=core-$dev
    echo "== $dev, from $f"
    compile "$dev"
    rm -rf "$BUILD/$dev-core" "$BUILD/$dev-mod"   # one .elemod each: an older version's would be taken too
    el -m elekloader.sdk.build "$EL/mods/$core" --stock "$syx" --out "$BUILD/$dev-core" | head -1
    el -m elekloader.sdk.build "$BUILD/$dev-manifest" --stock "$syx" --out "$BUILD/$dev-mod" | head -1
    el -m elekloader.lint "$BUILD/$dev-mod"/*.elemod --stock "$syx" --with "$BUILD/$dev-core"/*.elemod \
        ${LINT_WITH[@]+"${LINT_WITH[@]}"} | tail -1
    # elekloader writes its manifest and map beside the file: they stay in out/build, the firmware file goes to out/
    el -m elekloader.patch --stock "$syx" --mod "$BUILD/$dev-core"/*.elemod --mod "$BUILD/$dev-mod"/*.elemod \
        --out "$BUILD/macro-$dev.syx" --version MMDV | tail -1
    mv "$BUILD/macro-$dev.syx" "$OUT/macro-$dev.syx"
    # where the mod's symbols landed, for the emulator scenarios (the layout object is the cross-compile's)
    el - "$syx" "$BUILD/$dev-core"/*.elemod "$BUILD/$dev-mod"/*.elemod "$BUILD/macro-$dev.map.json" \
        "$BUILD/$dev-layout.o" "${CROSS}nm" <<'PY'
import json, subprocess, sys
from elekloader import patch
stock, core, mod, out, layout, nm = sys.argv[1:7]
_, manifest = patch.build(stock, [core, mod], 'MMDV', log=lambda *a: None)
secs = manifest['link']['layout']['sections']
m = json.load(open(mod))
syms = {k: secs['macro-mode ' + v[0]] + v[1] for k, v in m['symbols'].items() if v[0] in ('.run', '.bss')}
c = json.load(open(core))
syms['core_store_conflicts'] = secs['core ' + c['symbols']['core_store_conflicts'][0]] + \
    c['symbols']['core_store_conflicts'][1]
syms['layout'] = {l.split()[2][6:]: int(l.split()[0], 16)
                  for l in subprocess.run([nm, layout], capture_output=True, text=True).stdout.splitlines()
                  if ' A MMOFF_' in l}
json.dump(syms, open(out, 'w'), indent=1, sort_keys=True)
PY
    built+=("$OUT/macro-$dev.syx")
done

echo
echo "== firmware ready (send it to the unit as an OS update; keep it to yourself: it holds Elektron's OS)"
for b in "${built[@]}"; do
    echo "  $PWD/$b"
done
