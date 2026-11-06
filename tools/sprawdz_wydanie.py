#!/usr/bin/env python3
"""Sprawdza podpis binarnego wykazu VGKM i integralność plików wydania."""
import argparse
import sys
from pathlib import Path

from vgkm import VGKMError, decode_manifest, verify_files, verify_signature


def main() -> int:
    parser = argparse.ArgumentParser(description="Sprawdza podpis i integralność wydania VGK.")
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path, help="binarny plik VGKM")
    parser.add_argument("--signature", required=True, type=Path, help="64-bajtowy podpis VGKM.sig")
    parser.add_argument("--public-key", required=True, type=Path, help="zaufany klucz publiczny PEM")
    args = parser.parse_args()
    try:
        # Najpierw uwierzytelniamy dokładne bajty, dopiero potem interpretujemy ścieżki.
        verify_signature(args.manifest, args.signature, args.public_key)
        raw = args.manifest.read_bytes()
        records = decode_manifest(raw)
        root = args.root.resolve(strict=True)
        if not root.is_dir():
            raise VGKMError("katalog wydania nie istnieje lub nie jest katalogiem")
        verify_files(root, records)
    except (OSError, VGKMError) as exc:
        print(f"BŁĄD WERYFIKACJI: {exc}", file=sys.stderr)
        return 1
    print("OK: podpis binarnego wykazu VGKM jest poprawny.")
    print(f"OK: sprawdzono {len(records)} plików; nie wykryto zmian.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
