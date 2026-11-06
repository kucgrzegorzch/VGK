#!/usr/bin/env python3
"""Tworzy wykaz SHA-256 i podpisuje go kluczem wydawcy VGK."""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path, PurePosixPath


class SigningError(Exception):
    pass


def checked_file(root: Path, relative: str) -> Path:
    pure = PurePosixPath(relative)
    if not relative or pure.is_absolute() or any(p in ("", ".", "..") for p in pure.parts):
        raise SigningError(f"niedozwolona ścieżka: {relative}")
    if "\\" in relative or ":" in relative:
        raise SigningError(f"niedozwolona ścieżka: {relative}")
    current = root
    for part in pure.parts:
        current = current / part
        if current.is_symlink():
            raise SigningError(f"ścieżka prowadzi przez dowiązanie: {relative}")
    try:
        resolved = current.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError) as exc:
        raise SigningError(f"brak pliku lub ścieżka poza wydaniem: {relative}") from exc
    if not resolved.is_file():
        raise SigningError(f"to nie jest zwykły plik: {relative}")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(description="Tworzy i podpisuje wykaz plików wydania VGK.")
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--release", required=True)
    parser.add_argument("--private-key", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--signature", required=True, type=Path)
    parser.add_argument("files", nargs="+", help="ścieżki plików względem katalogu wydania")
    args = parser.parse_args()

    try:
        root = args.root.resolve(strict=True)
        if not root.is_dir():
            raise SigningError("katalog wydania nie istnieje")
        if not args.release.strip():
            raise SigningError("nazwa wydania nie może być pusta")
        if args.manifest.exists() or args.signature.exists():
            raise SigningError("plik wykazu lub podpisu już istnieje; nie zostanie nadpisany")
        if not args.private_key.is_file() or args.private_key.is_symlink():
            raise SigningError("klucz prywatny nie istnieje lub nie jest zwykłym plikiem")
        entries = []
        seen = set()
        for relative in args.files:
            if relative in seen:
                raise SigningError(f"powtórzona ścieżka: {relative}")
            seen.add(relative)
            path = checked_file(root, relative)
            digest = hashlib.sha256()
            size = 0
            with path.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    size += len(chunk)
                    digest.update(chunk)
            entries.append({"path": relative, "size": size, "sha256": digest.hexdigest()})

        manifest = {
            "format_version": 1,
            "release": args.release,
            "files": sorted(entries, key=lambda item: item["path"]),
        }
        raw = (json.dumps(manifest, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.signature.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_bytes(raw)
        command = [
            "openssl", "pkeyutl", "-sign", "-inkey", str(args.private_key),
            "-rawin", "-in", str(args.manifest), "-out", str(args.signature),
        ]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode != 0:
            args.manifest.unlink(missing_ok=True)
            detail = (result.stderr or result.stdout).strip()
            raise SigningError("nie udało się podpisać wykazu" + (f": {detail}" if detail else ""))
    except (OSError, SigningError) as exc:
        print(f"BŁĄD PRZYGOTOWANIA WYDANIA: {exc}", file=sys.stderr)
        return 1

    print(f"Utworzono wykaz {args.manifest} i podpis {args.signature}.")
    print("Klucz prywatny nie został skopiowany do katalogu wydania.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
