#!/usr/bin/env python3
"""Testy bezpiecznego tworzenia klucza zaufania wydawcy VGK."""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "przygotuj_klucz_zaufania.py"


class TrustedKeyGenerationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        base = Path(self.temp.name)
        self.private = base / "private.pem"
        self.public = base / "public.pem"
        self.header = base / "trusted_key.h"

    def run_generator(self):
        return subprocess.run(
            [sys.executable, str(SCRIPT),
             "--private-key", str(self.private),
             "--public-key", str(self.public),
             "--header", str(self.header)],
            capture_output=True, text=True, check=False)

    def test_creates_private_public_and_public_header(self):
        result = self.run_generator()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(self.private.is_file())
        self.assertTrue(self.public.is_file())
        self.assertTrue(self.header.is_file())
        self.assertEqual(self.private.stat().st_mode & 0o777, 0o600)
        header = self.header.read_text(encoding="ascii")
        values = re.findall(r"0x[0-9a-f]{2}", header)
        self.assertEqual(len(values), 32)
        self.assertNotIn("PRIVATE KEY", header)
        der = self.private.parent / "public.der"
        subprocess.run(
            ["openssl", "pkey", "-pubin", "-in", str(self.public),
             "-outform", "DER", "-out", str(der)],
            check=True, capture_output=True)
        raw = der.read_bytes()[-32:]
        self.assertEqual(bytes(int(value, 16) for value in values), raw)

    def test_refuses_to_overwrite_existing_files(self):
        self.private.write_text("do not replace", encoding="ascii")
        result = self.run_generator()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.private.read_text(encoding="ascii"), "do not replace")
        self.assertFalse(self.public.exists())
        self.assertFalse(self.header.exists())

    def test_preserves_old_predictable_temporary_path(self):
        old_temp = self.header.with_suffix(self.header.suffix + ".tmp.der")
        old_temp.write_bytes(b"keep this file")
        result = self.run_generator()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(old_temp.read_bytes(), b"keep this file")


if __name__ == "__main__":
    unittest.main(verbosity=2)
