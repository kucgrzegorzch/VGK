#!/usr/bin/env python3
"""Testy binarnego formatu VGKM używanego przez program uruchamiający."""
import hashlib
import tempfile
import unittest
from pathlib import Path

from vgkm import VGKMError, decode_manifest, encode_manifest, verify_files


class VGKMFormatTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "boot").mkdir()
        (self.root / "boot" / "kernel.elf").write_bytes(b"VGK-kernel")
        (self.root / "start.efi").write_bytes(b"EFI-start")

    def test_encoding_matches_binary_header_and_entry_layout(self):
        raw = encode_manifest(self.root, ["start.efi", "boot/kernel.elf"])
        self.assertEqual(raw[:8], b"VGKM\x01\x00\x02\x00")
        records = decode_manifest(raw)
        self.assertEqual([item[0] for item in records],
                         ["boot/kernel.elf", "start.efi"])
        verify_files(self.root, records)

    def test_detects_changed_file(self):
        raw = encode_manifest(self.root, ["boot/kernel.elf"])
        records = decode_manifest(raw)
        (self.root / "boot" / "kernel.elf").write_bytes(b"modified")
        with self.assertRaises(VGKMError):
            verify_files(self.root, records)

    def test_rejects_unsafe_paths(self):
        for path in ("../outside", "/absolute", "boot//kernel", "boot/../x",
                     "boot\\kernel", "boot:kernel"):
            with self.subTest(path=path):
                with self.assertRaises(VGKMError):
                    encode_manifest(self.root, [path])

    def test_rejects_duplicate_paths(self):
        with self.assertRaises(VGKMError):
            encode_manifest(self.root, ["start.efi", "start.efi"])

    def test_rejects_corrupt_or_trailing_manifest_bytes(self):
        raw = encode_manifest(self.root, ["start.efi"])
        with self.assertRaises(VGKMError):
            decode_manifest(raw + b"x")
        with self.assertRaises(VGKMError):
            decode_manifest(b"BAD!" + raw[4:])

    def test_hash_is_sha256(self):
        raw = encode_manifest(self.root, ["start.efi"])
        record = decode_manifest(raw)[0]
        self.assertEqual(record[2], hashlib.sha256(b"EFI-start").digest())


if __name__ == "__main__":
    unittest.main(verbosity=2)
