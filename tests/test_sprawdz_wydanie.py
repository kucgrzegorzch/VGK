#!/usr/bin/env python3
"""Testy przygotowania i sprawdzania binarnego wydania VGK."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREPARER = ROOT / "tools" / "przygotuj_wydanie.py"
VERIFIER = ROOT / "tools" / "sprawdz_wydanie.py"


class BinaryReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.release = self.base / "release"
        (self.release / "boot").mkdir(parents=True)
        self.payload = self.release / "boot" / "kernel.elf"
        self.payload.write_bytes(b"VGK test kernel")
        self.manifest = self.base / "VGKM"
        self.signature = self.base / "VGKM.sig"
        self.private = self.base / "private.pem"
        self.public = self.base / "public.pem"
        subprocess.run(["openssl", "genpkey", "-algorithm", "Ed25519",
                        "-out", str(self.private)], check=True, capture_output=True)
        subprocess.run(["openssl", "pkey", "-in", str(self.private),
                        "-pubout", "-out", str(self.public)], check=True, capture_output=True)
        self.prepare()

    def prepare(self):
        return subprocess.run(
            [sys.executable, str(PREPARER), "--root", str(self.release),
             "--release", "VGK-test", "--private-key", str(self.private),
             "--manifest", str(self.manifest), "--signature", str(self.signature),
             "boot/kernel.elf"], capture_output=True, text=True, check=False)

    def verify(self):
        return subprocess.run(
            [sys.executable, str(VERIFIER), "--root", str(self.release),
             "--manifest", str(self.manifest), "--signature", str(self.signature),
             "--public-key", str(self.public)], capture_output=True, text=True, check=False)

    def test_generator_emits_binary_vgkm_and_valid_signature(self):
        self.assertEqual(self.manifest.read_bytes()[:8], b"VGKM\\x01\\x00\\x01\\x00")
        self.assertEqual(self.signature.stat().st_size, 64)
        self.assertEqual(self.verify().returncode, 0)

    def test_refuses_overwrite(self):
        before = self.manifest.read_bytes()
        result = self.prepare()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.manifest.read_bytes(), before)

    def test_detects_modified_file(self):
        self.payload.write_bytes(b"changed kernel!")
        result = self.verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("niezgodna zawartość", result.stderr)

    def test_detects_modified_manifest(self):
        self.manifest.write_bytes(self.manifest.read_bytes() + b"x")
        self.assertNotEqual(self.verify().returncode, 0)

    def test_detects_bad_signature(self):
        data = bytearray(self.signature.read_bytes())
        data[0] ^= 1
        self.signature.write_bytes(data)
        self.assertNotEqual(self.verify().returncode, 0)

    def test_rejects_symlink(self):
        self.payload.unlink()
        outside = self.base / "outside"
        outside.write_bytes(b"VGK test kernel")
        self.payload.symlink_to(outside)
        result = self.prepare()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("dowiązanie", result.stderr)

    def test_rejects_path_traversal(self):
        result = subprocess.run(
            [sys.executable, str(PREPARER), "--root", str(self.release),
             "--release", "bad", "--private-key", str(self.private),
             "--manifest", str(self.base / "bad.VGKM"),
             "--signature", str(self.base / "bad.sig"), "../outside"],
            capture_output=True, text=True, check=False)
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
