#!/usr/bin/env python3
"""Tworzy binarny wykaz VGKM zgodny z parserem programu uruchamiającego."""
import argparse
import sys
from pathlib import Path

from vgkm import VGKMError, encode_manifest, sign_manifest


def main() -> int:
    parser = argparse.ArgumentParser(description="Tworzy i podpisuje binarny wykaz VGKM.")
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--release", required=True, help="etykieta wydania (informacyjna)")
    parser.add_argument("--private-key", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path, help="plik binarny VGKM")
    parser.add_argument("--signature", required=True, type=Path, help="64-bajtowy podpis VGKM.sig")
    parser.add_argument("files", nargs="+", help="ścieżki względem katalogu wydania")
    args = parser.parse_args()
    try:
        if not args.release.strip():
            raise VGKMError("nazwa wydania nie może być pusta")
        if args.manifest.exists() or args.signature.exists():
            raise VGKMError("plik wykazu lub podpisu już istnieje; nie zostanie nadpisany")
        if args.private_key.is_symlink() or not args.private_key.is_file():
            raise VGKMError("klucz prywatny nie istnieje lub nie jest zwykłym plikiem")
        raw = encode_manifest(args.root, args.files)
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.signature.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_bytes(raw)
        try:
            sign_manifest(args.manifest, args.private_key, args.signature)
        except Exception:
            args.manifest.unlink(missing_ok=True)
            raise
    except (OSError, VGKMError) as exc:
        print(f"BŁĄD PRZYGOTOWANIA WYDANIA: {exc}", file=sys.stderr)
        return 1
    print(f"Utworzono binarny wykaz VGKM wydania „{args.release}” oraz podpis.")
    print("Klucz prywatny nie został skopiowany do katalogu wydania.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
