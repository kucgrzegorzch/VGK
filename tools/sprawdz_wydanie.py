#!/usr/bin/env python3
"""Sprawdza podpisany wykaz i integralność plików wydania VGK."""

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath


class VerificationError(Exception):
    pass


def reject_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise VerificationError(f"powtórzone pole JSON: {key}")
        result[key] = value
    return result


def load_manifest(path: Path):
    try:
        raw = path.read_bytes()
        manifest = json.loads(raw.decode("utf-8"), object_pairs_hook=reject_duplicates)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise VerificationError(f"nie można odczytać wykazu: {exc}") from exc
    if not isinstance(manifest, dict):
        raise VerificationError("wykaz musi być obiektem JSON")
    if set(manifest) != {"format_version", "release", "files"}:
        raise VerificationError("nieprawidłowy zestaw pól wykazu")
    if type(manifest["format_version"]) is not int or manifest["format_version"] != 1:
        raise VerificationError("nieobsługiwana wersja formatu wykazu")
    if not isinstance(manifest["release"], str) or not manifest["release"].strip():
        raise VerificationError("brak nazwy wydania")
    if not isinstance(manifest["files"], list) or not manifest["files"]:
        raise VerificationError("wykaz nie zawiera plików")
    return raw, manifest


def safe_file(root: Path, relative: str) -> Path:
    if not isinstance(relative, str) or not relative:
        raise VerificationError("ścieżka pliku jest pusta lub nie jest tekstem")
    pure = PurePosixPath(relative)
    if pure.is_absolute() or any(part in ("", ".", "..") for part in pure.parts):
        raise VerificationError(f"niedozwolona ścieżka: {relative}")
    if "\\" in relative or ":" in relative:
        raise VerificationError(f"niedozwolona ścieżka: {relative}")

    current = root
    for part in pure.parts:
        current = current / part
        if current.is_symlink():
            raise VerificationError(f"ścieżka prowadzi przez dowiązanie: {relative}")
    try:
        resolved = current.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError) as exc:
        raise VerificationError(f"plik poza katalogiem wydania lub brak pliku: {relative}") from exc
    if not resolved.is_file():
        raise VerificationError(f"to nie jest zwykły plik: {relative}")
    return resolved


def verify_signature(manifest_path: Path, signature_path: Path, public_key: Path):
    command = [
        "openssl", "pkeyutl", "-verify", "-pubin", "-inkey", str(public_key),
        "-sigfile", str(signature_path), "-rawin", "-in", str(manifest_path),
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=False)
    except FileNotFoundError as exc:
        raise VerificationError("brak programu OpenSSL potrzebnego do sprawdzenia podpisu") from exc
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise VerificationError("podpis wykazu jest nieprawidłowy" + (f": {detail}" if detail else ""))


def verify_files(root: Path, manifest):
    seen = set()
    for item in manifest["files"]:
        if not isinstance(item, dict) or set(item) != {"path", "size", "sha256"}:
            raise VerificationError("każdy wpis musi zawierać wyłącznie path, size i sha256")
        relative = item["path"]
        if relative in seen:
            raise VerificationError(f"powtórzona ścieżka: {relative}")
        seen.add(relative)
        if type(item["size"]) is not int or item["size"] < 0:
            raise VerificationError(f"nieprawidłowy rozmiar pliku: {relative}")
        digest = item["sha256"]
        if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise VerificationError(f"nieprawidłowy skrót SHA-256: {relative}")
        path = safe_file(root, relative)
        actual_size = path.stat().st_size
        if actual_size != item["size"]:
            raise VerificationError(f"niezgodny rozmiar pliku: {relative}")
        actual_digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual_digest != digest:
            raise VerificationError(f"niezgodna zawartość pliku: {relative}")


def main():
    parser = argparse.ArgumentParser(description="Sprawdza podpis i integralność wydania VGK.")
    parser.add_argument("--root", required=True, type=Path, help="katalog główny sprawdzanego wydania")
    parser.add_argument("--manifest", required=True, type=Path, help="podpisany wykaz JSON")
    parser.add_argument("--signature", required=True, type=Path, help="podpis Ed25519 wykazu")
    parser.add_argument("--public-key", required=True, type=Path, help="zaufany klucz publiczny PEM")
    args = parser.parse_args()

    try:
        raw, manifest = load_manifest(args.manifest)
        verify_signature(args.manifest, args.signature, args.public_key)
        root = args.root.resolve(strict=True)
        if not root.is_dir():
            raise VerificationError("katalog wydania nie istnieje lub nie jest katalogiem")
        verify_files(root, manifest)
    except (VerificationError, OSError) as exc:
        print(f"BŁĄD WERYFIKACJI: {exc}", file=sys.stderr)
        return 1

    print(f"OK: podpis wydania „{manifest['release']}” jest poprawny.")
    print(f"OK: sprawdzono {len(manifest['files'])} plików; nie wykryto zmian.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
