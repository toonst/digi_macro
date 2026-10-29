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
SETUP_SIZE = 194   # MM_SETUP_SIZE; tools/check.sh checks the built manifests with the size the build has


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
    args = [sys.executable, 'tools/storage.py', str(d), '--setup-size', str(SETUP_SIZE)]
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

    def test_a_claim_short_of_the_setups_is_refused(self):
        def shrink(m):
            m['resources']['names'] = [n.replace('0x573e-0x5800', '0x5740-0x5800') for n in m['resources']['names']]
        code, out = check(built(['kits'], shrink))
        self.assertEqual(code, 1)
        self.assertIn('KIT_PAD', out)

    def test_a_mod_elsewhere_passes(self):
        code, out = check(self.kits, 'clean', 'other-device')
        self.assertEqual(code, 0, out)

    def test_overlapping_claims_conflict(self):
        for other, ours in (('kit-overlap', 'record:kit'), ('project-overlap', 'record:project'),
                            ('card-overlap', 'card')):
            code, out = check(self.kits, other)
            self.assertEqual(code, 1, other)
            self.assertIn('overlaps this mod\'s ' + ours, out)

    def test_card_overlap_without_the_feature_too(self):
        self.assertEqual(check(self.plain, 'card-overlap')[0], 1)
        self.assertEqual(check(self.plain, 'kit-overlap')[0], 0)   # no kit storage without the feature

    def test_a_site_in_a_storage_routine_conflicts(self):
        for other, routine in (('kit-routine', 'kit record save'), ('memcpy-routine', 'memcpy entry')):
            code, out = check(self.kits, other)
            self.assertEqual(code, 1, other)
            self.assertIn(routine, out)
        self.assertEqual(check(self.plain, 'kit-routine')[0], 0)

    def test_a_malformed_claim_is_reported(self):
        code, out = check(self.kits, 'malformed')
        self.assertEqual(code, 1)
        self.assertIn('not SPACE:LO-HI', out)


if __name__ == '__main__':
    unittest.main()
