import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('fetch_runtime_data', ROOT / 'scripts/fetch_runtime_data.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

# Their only recorded sources are third-party mirrors, so no observed hash has
# been pinned for them; everything else must be pinned.
UNPINNED_AWAITING_SOURCE = {'cave-story-english-1006', 'jazz-shareware-1.1', 'pop1-demo'}


class RuntimeDataTests(unittest.TestCase):
    def test_every_upstream_linked_dataset_is_pinned(self):
        manifest = MODULE.load_manifest(ROOT)
        unpinned = {name for name, spec in manifest['datasets'].items() if not spec.get('sha256')}
        self.assertEqual(unpinned, UNPINNED_AWAITING_SOURCE)
        for name, spec in manifest['datasets'].items():
            if spec.get('sha256'):
                self.assertRegex(spec['sha256'], r'^[0-9a-f]{64}$', name)

    def test_pinned_dataset_rejects_other_content(self):
        manifest = MODULE.load_manifest(ROOT)
        spec = manifest['datasets']['omf2097-assets']
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / spec['filename']).write_bytes(b'not the OpenOMF assets')
            result = MODULE.fetch_one('omf2097-assets', spec, cache, allow_network=False)
            self.assertEqual(result['state'], 'missing-network-disabled')
            self.assertFalse((cache / spec['filename']).exists())


if __name__ == '__main__':
    unittest.main()
