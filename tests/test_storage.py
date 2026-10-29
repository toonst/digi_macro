"""tools/storage.py against the fixtures in tests/storage: the mod's storage claims against other mods (README.md,
"Storage and other mods"). No firmware needed. Run by tools/check.sh:

    python3 tests/test_storage.py
"""
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
FIX = ROOT / 'tests' / 'storage'


def built(features, edit=None):
    """A manifest dir as tools/manifest.py writes it, for the Digitakt II, optionally edited."""
    d = pathlib.Path(tempfile.mkdtemp())
    subprocess.run([sys.executable, 'tools/manifest.py', 'dt2', str(d)] + features, cwd=ROOT, check=True)
    if edit:
        m = json.load(open(d / 'mod.json'))
        edit(m)
        json.dump(m, open(d / 'mod.json', 'w'))
    return d


def check(d, *others):
    args = [sys.executable, 'tools/storage.py', str(d)]
    for o in others:
        args += ['--with', str(FIX / (o + '.json'))]
    r = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    return r.returncode, r.stdout


class Storage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.kits = built(['kits'])
        cls.plain = built([])

    def test_own_claims_cover_what_the_code_writes(self):
        for d in (self.kits, self.plain):
            code, out = check(d)
            self.assertEqual(code, 0, out)

    def test_a_card_claim_short_of_the_sectors_is_refused(self):
        def shrink(m):
            m['resources']['names'] = [n.replace('0x2df0e00-0x2df0e02', '0x2df0e00-0x2df0e01')
                                       for n in m['resources']['names']]
        code, out = check(built(['kits'], shrink))
        self.assertEqual(code, 1)
        self.assertIn('two card sectors', out)

    def test_a_mod_elsewhere_passes(self):
        code, out = check(self.kits, 'clean', 'other-device')
        self.assertEqual(code, 0, out)

    def test_the_same_data_or_overlapping_sectors_conflict(self):
        for other, ours in (('kit-overlap', 'kitdata:MMKS too'), ('project-overlap', 'projdata:MMPS too'),
                            ('card-overlap', 'overlaps this mod\'s card')):
            code, out = check(self.kits, other)
            self.assertEqual(code, 1, other)
            self.assertIn(ours, out)

    def test_without_the_feature_only_the_card_is_ours(self):
        self.assertEqual(check(self.plain, 'card-overlap')[0], 1)
        self.assertEqual(check(self.plain, 'kit-overlap')[0], 0)

    def test_a_malformed_claim_is_reported(self):
        code, out = check(self.kits, 'malformed')
        self.assertEqual(code, 1)
        self.assertIn('not card:0xLO-0xHI', out)


if __name__ == '__main__':
    unittest.main()
