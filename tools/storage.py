"""The mod's storage claims against itself and against other mods (docs/DEVELOPING.md, "Storage and other mods").

The mod claims every place it keeps data outside its own RAM as a resource name: `card:0xLO-0xHI` (eMMC sectors,
half-open), and with the kits feature `kitdata:TAG` and `projdata:TAG` (its blocks in elekloader core's kit and
project data). elekloader's linker refuses two mods with the same name or overlapping ranges of one space when they
are built together; this checks the same from the manifests alone, without the stock files:

- ours: each card claim is well formed, and the card sectors the code writes (CARD_SECTOR, two) lie inside one;
- with --with OTHER (a mod.json or an .elemod, for the same device): no card claim of OTHER overlaps ours, and OTHER
  claims none of our kit or project data tags.

    python3 tools/storage.py BUILT_MANIFEST_DIR [--with OTHER ...]

Exit status 1 on a problem, each printed.
"""
import argparse
import json
import pathlib
import re
import sys

CARD = re.compile(r'^card:(0x[0-9a-fA-F]+)-(0x[0-9a-fA-F]+)$')
TAGS = ('kitdata:', 'projdata:')


def claims(doc, who, problems):
    """(cards [(lo, hi, name)], tags [name]) of a manifest's storage claims; a malformed card claim is a problem."""
    cards, tags = [], []
    for nm in (doc.get('resources') or {}).get('names', []):
        m = CARD.match(nm)
        if m and int(m.group(1), 16) < int(m.group(2), 16):
            cards.append((int(m.group(1), 16), int(m.group(2), 16), nm))
        elif nm.startswith('card:'):
            problems.append('%s: %s is not card:0xLO-0xHI with LO < HI' % (who, nm))
        elif nm.startswith(TAGS):
            tags.append(nm)
    return cards, tags


def cflag(doc, name):
    for f in doc.get('cflags', []):
        if f.startswith('-D%s=' % name):
            return int(f.split('=', 1)[1], 0)
    return None


def device(doc):
    return doc.get('device') or (doc.get('target') or {}).get('device')


def check(mdir, others):
    problems = []
    ours = json.load(open(mdir / 'mod.json'))
    cards, tags = claims(ours, 'this mod', problems)
    sector = cflag(ours, 'CARD_SECTOR')
    if sector is not None and not any(lo <= sector and sector + 2 <= hi for lo, hi, _ in cards):
        problems.append('this mod: the configuration\'s two card sectors (0x%x) are not inside a claim of its own'
                        % sector)
    for path in others:
        doc = json.load(open(path))
        who = '%s (%s)' % (doc.get('id', '?'), path)
        if device(doc) and device(ours) and device(doc) != device(ours):
            continue
        theirs, their_tags = claims(doc, who, problems)
        for lo, hi, nm in theirs:
            for mlo, mhi, mnm in cards:
                if lo < mhi and mlo < hi:
                    problems.append('%s claims %s, which overlaps this mod\'s %s' % (who, nm, mnm))
        for nm in set(their_tags) & set(tags):
            problems.append('%s claims %s too' % (who, nm))
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('manifest_dir', type=pathlib.Path)
    ap.add_argument('--with', dest='others', action='append', default=[])
    a = ap.parse_args()
    problems = check(a.manifest_dir, a.others)
    for p in problems:
        print('storage: ' + p)
    if not problems:
        print('storage: ok, %d other mod(s) checked' % len(a.others))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
