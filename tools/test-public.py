#!/usr/bin/env python3
"""Offline checks for public export boundaries and configuration validation."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


def module(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


check = module('check-public')
configure = module('configure-device')
exporter = module('export-public')


class PublicTests(unittest.TestCase):
    def test_credentials_and_private_files_are_flagged_without_values(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'private').mkdir()
            (root / 'private' / 'config.json').write_text(json.dumps({'apiKey': 'sk-proj-' + 'x' * 48}))
            issues = check.inspect(root)
            self.assertTrue(any('private/generated directory' in issue for issue in issues))
            self.assertTrue(any('populated credential' in issue for issue in issues))
            self.assertFalse(any('x' * 48 in issue for issue in issues))

    def test_symlinks_and_broken_links_are_flagged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'README.md').write_text('[Missing](docs/missing.md)')
            (root / 'outside').symlink_to('/etc/hostname')
            issues = check.inspect(root)
            self.assertTrue(any('symlinks' in issue for issue in issues))
            self.assertTrue(any('broken or external local link' in issue for issue in issues))

    def test_allowlist_excludes_recordings_models_and_local_state(self):
        for path in exporter.files():
            relative = path.relative_to(exporter.ROOT)
            self.assertFalse(set(relative.parts) & {'private', 'build', 'artifacts', 'upstream'})
            self.assertNotIn(path.suffix, {'.wav', '.onnx', '.tflite', '.img', '.key', '.pem'})
            self.assertNotIn(path.name, {'admin.md', 'SETUP.md', 'spec.md'})

    def test_invalid_module_and_location_configuration_is_rejected(self):
        for value in ({'modules': {'assistant': 'false'}}, {'modules': {'unknown': True}},
                      {'weather': {'latitude': 91}}, {'weather': {'timezone': 'Invalid/Zone'}},
                      {'apiKey': 'belongs-in-a-private-file'}):
            with self.assertRaises(ValueError):
                configure.validate(value)
        configure.validate({'version': 1, 'modules': {'assistant': False}, 'weather': {'latitude': 1, 'longitude': 2, 'timezone': 'UTC'}})


if __name__ == '__main__':
    unittest.main()
