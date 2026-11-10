"""The manifest a build uses: mods/<dev>/mod.json, the mod without its optional features, plus each chosen feature's
fragment mods/<dev>/<feature>.json (docs/DEVELOPING.md, "Features"). Lists (sources, cflags, subscribe, contribute,
sites, resource names) are joined, tables (defsym, collections) merged; a name both define is an error. A feature's
`resources.drop_names` takes claims of mod.json away (the project feature writes no card sector). The sources
are rewritten for the output folder, which elekloader's build and tools/check.sh's cross-compile then take as the
mod's folder.

    python3 tools/manifest.py DEV OUT_DIR [FEATURE ...] [--cflag FLAG ...]
"""
import argparse
import json
import os
import pathlib
import sys

LISTS = ('sources', 'cflags', 'subscribe', 'contribute', 'sites')
TABLES = ('defsym', 'collections')


def merge(base, frag, name):
    for key in LISTS:
        base.setdefault(key, []).extend(frag.get(key, []))
    for key in TABLES:
        table = base.setdefault(key, {})
        for k, v in frag.get(key, {}).items():
            if k in table and table[k] != v:
                sys.exit('%s: %s %s is already %s' % (name, key, k, table[k]))
            table[k] = v
    names = base.setdefault('resources', {}).setdefault('names', [])
    names.extend(frag.get('resources', {}).get('names', []))
    for gone in frag.get('resources', {}).get('drop_names', []):    # a claim of mod.json's the feature gives up
        if gone not in names:
            sys.exit('%s: drop_names: %s is not a claim of mod.json' % (name, gone))
        names.remove(gone)
    unknown = set(frag) - set(LISTS) - set(TABLES) - {'resources', '_comment'}
    if unknown:
        sys.exit('%s: keys a feature cannot set: %s' % (name, ', '.join(sorted(unknown))))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dev')
    ap.add_argument('out')
    ap.add_argument('features', nargs='*')
    ap.add_argument('--cflag', action='append', default=[])
    a = ap.parse_args()
    src = pathlib.Path('mods') / a.dev
    m = json.load(open(src / 'mod.json'))
    for f in a.features:
        merge(m, json.load(open(src / (f + '.json'))), '%s/%s.json' % (src, f))
    m['cflags'] += a.cflag
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    m['sources'] = [os.path.relpath(src / s, out) for s in m['sources']]
    with open(out / 'mod.json', 'w') as fh:
        json.dump(m, fh, indent=1)
        fh.write('\n')


if __name__ == '__main__':
    main()
