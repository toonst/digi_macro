"""The mod's storage against itself and against other mods (README.md, "Storage and other mods").

The mod claims every place it keeps data outside its own RAM as a resource name with a half-open range:
`card:LO-HI` (eMMC sectors), `record:kit:LO-HI` (bytes of a saved kit record) and `record:project:LO-HI` (bytes of a
saved project file). elekloader refuses two mods that claim the same name, but compares names as text; this checks
the ranges:

- ours: each claim is well formed, the card sectors and the setups the code writes (CARD_SECTOR, KIT_PAD, PROJ_PAD
  and the setup's size) lie inside what is claimed;
- with --with OTHER (a mod.json or an .elemod, for the same device): no claim of OTHER overlaps one of ours in the
  same space, and none of OTHER's sites is inside a firmware routine our storage goes through (storage.json, from
  tools/manifest.py), where it could write or move the same bytes.

    python3 tools/storage.py BUILT_MANIFEST_DIR --setup-size N [--with OTHER ...]

Exit status 1 on a problem, each printed.
"""
import argparse
import json
import pathlib
import re
import sys

CLAIM = re.compile(r'^(card|record:kit|record:project):(0x[0-9a-fA-F]+)-(0x[0-9a-fA-F]+)$')
SPACES = ('card:', 'record:')


def claims(doc, who, problems):
    """(space, lo, hi, name) of a manifest's storage claims; a malformed one in a space this convention owns is a
    problem."""
    out = []
    for nm in (doc.get('resources') or {}).get('names', []):
        m = CLAIM.match(nm)
        if m:
            lo, hi = int(m.group(2), 16), int(m.group(3), 16)
            if lo < hi:
                out.append((m.group(1), lo, hi, nm))
                continue
        if nm.startswith(SPACES):
            problems.append('%s: %s is not SPACE:LO-HI with LO < HI (card, record:kit, record:project)' % (who, nm))
    return out


def cflag(doc, name):
    for f in doc.get('cflags', []):
        if f.startswith('-D%s=' % name):
            return int(f.split('=', 1)[1], 0)
    return None


def site_span(s):
    lo = int(str(s['addr']), 0)
    n = s.get('len') or len(bytes.fromhex(s.get('stock', '')))
    return lo, lo + n


def device(doc):
    return doc.get('device') or (doc.get('target') or {}).get('device')


def check(mdir, setup_size, others):
    problems = []
    ours = json.load(open(mdir / 'mod.json'))
    routines = json.load(open(mdir / 'storage.json')).get('routines', {}) if (mdir / 'storage.json').exists() else {}
    mine = claims(ours, 'this mod', problems)

    def inside(space, lo, hi, what):
        if not any(sp == space and a <= lo and hi <= b for sp, a, b, _ in mine):
            problems.append('this mod: %s (%s 0x%x-0x%x) is not inside a claim of its own' % (what, space, lo, hi))

    sector = cflag(ours, 'CARD_SECTOR')
    if sector is not None:
        inside('card', sector, sector + 2, 'the configuration\'s two card sectors')
    for name, space in (('KIT_PAD', 'record:kit'), ('PROJ_PAD', 'record:project')):
        pad = cflag(ours, name)
        if pad is not None:
            inside(space, pad, pad + setup_size, 'the setups at %s' % name)
    for i, a in enumerate(mine):            # our own claims must not overlap each other either
        for b in mine[i + 1:]:
            if a[0] == b[0] and a[1] < b[2] and b[1] < a[2]:
                problems.append('this mod: %s and %s overlap' % (a[3], b[3]))

    for path in others:
        doc = json.load(open(path))
        who = '%s (%s)' % (doc.get('id', '?'), path)
        if device(doc) and device(ours) and device(doc) != device(ours):
            continue
        for sp, lo, hi, nm in claims(doc, who, problems):
            for msp, mlo, mhi, mnm in mine:
                if sp == msp and lo < mhi and mlo < hi:
                    problems.append('%s claims %s, which overlaps this mod\'s %s' % (who, nm, mnm))
        for s in doc.get('sites', []):
            lo, hi = site_span(s)
            for what, (a, b) in routines.items():
                if lo < int(b, 16) and int(a, 16) < hi:
                    problems.append('%s changes 0x%08x-0x%08x, inside the %s routine (%s-%s) this mod\'s storage '
                                    'goes through' % (who, lo, hi, what, a, b))
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('manifest_dir', type=pathlib.Path)
    ap.add_argument('--setup-size', type=int, required=True)
    ap.add_argument('--with', dest='others', action='append', default=[])
    a = ap.parse_args()
    problems = check(a.manifest_dir, a.setup_size, a.others)
    for p in problems:
        print('storage: ' + p)
    if not problems:
        print('storage: ok, %d other mod(s) checked' % len(a.others))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
