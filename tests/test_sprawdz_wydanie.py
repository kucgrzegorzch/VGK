#!/usr/bin/env python3
"""Testy narzędzia sprawdzającego podpisane wydanie VGK."""

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERIFIER = ROOT / "tools" / "sprawdz_wydanie.py"


class ReleaseVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.release = self.base / "wydanie"
        self.release.mkdir()
        self.payload = self.release / "boot" / "vgk-kernel.elf"
        self.payload.parent.mkdir()
        self.payload.write_bytes(b"VGK test kernel\x00")
        self.manifest = self.base / "wykaz.json"
        self.signature = self.base / "wykaz.podpis"
        self.private = self.base / "klucz-prywatny.pem"
        self.public = self.base / "klucz-publiczny.pem"
        subprocess.run(
            ["openssl", "genpkey", "-algorithm", "Ed25519", "-out", str(self.private)],
            check=True, capture_output=True,
        )
        subprocess.run(
            ["openssl", "pkey", "-in", str(self.private), "-pubout", "-out", str(self.public)],
            check=True, capture_output=True,
        )
        self.write_manifest()
        self.sign()

    def write_manifest(self):
        data = self.payload.read_bytes()
        manifest = {
            "format_version": 1,
            "release": "VGK-test-1",
            "files": [{
                "path": "boot/vgk-kernel.elf",
                "size": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }],
        }
        self.manifest.write_text(json.dumps(manifest, separators=(",", ":")) + "\n", encoding="utf-8")

    def sign(self):
        subprocess.run(
            ["openssl", "pkeyutl", "-sign", "-inkey", str(self.private),
             "-rawin", "-in", str(self.manifest), "-out", str(self.signature)],
            check=True, capture_output=True,
        )

    def run_verifier(self):
        return subprocess.run(
            [sys.executable, str(VERIFIER), "--root", str(self.release),
             "--manifest", str(self.manifest), "--signature", str(self.signature),
             "--public-key", str(self.public)],
            capture_output=True, text=True, check=False,
        )

    def test_accepts_unchanged_signed_file(self):
        result = self.run_verifier()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_changed_file(self):
        self.payload.write_bytes(b"XGK test kernel" + bytes([0]))
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("niezgodna zawartość", result.stderr)

    def test_rejects_modified_manifest(self):
        self.manifest.write_bytes(self.manifest.read_bytes() + b" ")
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("podpis wykazu", result.stderr)

    def test_rejects_non_text_path(self):
        data = json.loads(self.manifest.read_text(encoding="utf-8"))
        data["files"][0]["path"] = ["boot", "vgk-kernel.elf"]
        self.manifest.write_text(json.dumps(data), encoding="utf-8")
        self.sign()
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("ścieżka pliku", result.stderr)

    def test_rejects_tampered_signature(self):
        signature = bytearray(self.signature.read_bytes())
        signature[0] ^= 1
        self.signature.write_bytes(signature)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("podpis wykazu", result.stderr)

    def test_rejects_symlinked_file(self):
        self.payload.unlink()
        outside = self.base / "outside.bin"
        outside.write_bytes(b"VGK test kernel" + bytes([0]))
        self.payload.symlink_to(outside)
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("dowiązanie", result.stderr)

    def test_rejects_path_traversal(self):
        data = json.loads(self.manifest.read_text(encoding="utf-8"))
        data["files"][0]["path"] = "../poza.txt"
        self.manifest.write_text(json.dumps(data), encoding="utf-8")
        self.sign()
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("niedozwolona ścieżka", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
