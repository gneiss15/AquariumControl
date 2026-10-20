#!/usr/bin/env python3
# test_public_release.py, Version: 1.02

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('release', Path(__file__).resolve().parents[1] / 'PreparePublicRelease.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ExportTests(unittest.TestCase):
    def fixture(self, root):
        (root / 'Aq/Test/DevState').mkdir(parents=True)
        (root / 'Aq/Test/DevState/Config.json').write_text('{"WlanPw":"private-test-value"}')
        (root / 'Aq/Test/TestConfig.inc.h').write_text('CD(T, String, ApPw, "private-test-value")\n')
        (root / 'Aq/Test/Versions.txt').write_text('App=1.00\n')
        (root / 'Aq/Test/hw_intern').mkdir()
        (root / 'Aq/Test/hw_intern/notes.md').write_text('INTERNAL_HARDWARE_ONLY')
        (root / 'Aq/Test/partitions.csv').write_text('# partition definition\n')
        (root / 'Aq/Test/old.zip').write_bytes(b'archive')
        (root / 'Aq/Test/private-link.h').symlink_to(root / 'Aq/Test/DevState/Config.json')
        (root / 'README.public.md').write_text('# Example\n')
        (root / 'BUILD-SECRETS.md').write_text('# Private build setup\n')

    def test_exclusions_sanitization_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            output = root / 'review'
            release.prepare(root, output)
            with zipfile.ZipFile(output / 'AquariumSources.review.zip') as archive:
                self.assertNotIn('Aq/Test/DevState/Config.json', archive.namelist())
                self.assertNotIn('Aq/Test/private-link.h', archive.namelist())
                self.assertNotIn('Aq/Test/old.zip', archive.namelist())
                self.assertFalse(any('/hw_intern/' in name for name in archive.namelist()))
                self.assertIn(b'**/hw_intern/', archive.read('.gitignore'))
                self.assertIn('Aq/Test/partitions.csv', archive.namelist())
                self.assertIn(b'CHANGE_ME', archive.read('Aq/Test/TestConfig.inc.h'))
                self.assertNotIn(b'private-test-value', b''.join(archive.read(name) for name in archive.namelist()))
            self.assertIn('private-test-value', (root / 'Aq/Test/TestConfig.inc.h').read_text())
            with self.assertRaises(RuntimeError):
                release.prepare(root, output)

    def test_secret_json_blocks_archive_without_logging_value(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            (root / 'Aq/Test/unsafe.json').write_text('{"FtpPw":"private-test-value"}')
            output = root / 'review'
            with self.assertRaises(RuntimeError):
                release.prepare(root, output)
            self.assertFalse((output / 'AquariumSources.review.zip').exists())
            self.assertNotIn('private-test-value', (output / 'Review.json').read_text())
            self.assertTrue(json.loads((output / 'Review.json').read_text())['Findings'])

    def test_hardware_assets_preserved_but_backups_and_manuals_excluded(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            hardware = root / 'Aq/Test/Hw'
            hardware.mkdir()
            (hardware / 'Photo.png').write_bytes(b'photo fixture')
            (hardware / 'Old.FCBak').write_bytes(b'private backup')
            (hardware / 'Manual.pdf').write_bytes(b'manufacturer manual')
            with zipfile.ZipFile(hardware / 'Model.FCStd', 'w') as model:
                model.writestr('Document.xml', '<Document/>')
            output = root / 'review'
            release.prepare(root, output)
            with zipfile.ZipFile(output / 'AquariumSources.review.zip') as archive:
                self.assertEqual(archive.read('Aq/Test/Hw/Photo.png'), b'photo fixture')
                self.assertEqual(archive.read('Aq/Test/Hw/Model.FCStd'),
                                 (hardware / 'Model.FCStd').read_bytes())
                self.assertNotIn('Aq/Test/Hw/Old.FCBak', archive.namelist())
                self.assertNotIn('Aq/Test/Hw/Manual.pdf', archive.namelist())

    def test_embedded_private_key_in_cad_blocks_export(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            hardware = root / 'Aq/Test/Hw'
            hardware.mkdir()
            with zipfile.ZipFile(hardware / 'Model.FCStd', 'w') as model:
                model.writestr('Document.xml', '<Document>' + '-' * 5 +
                               'BEGIN PRIVATE KEY' + '-' * 5 + '</Document>')
            output = root / 'review'
            with self.assertRaises(RuntimeError):
                release.prepare(root, output)
            self.assertFalse((output / 'AquariumSources.review.zip').exists())


if __name__ == '__main__':
    unittest.main()
