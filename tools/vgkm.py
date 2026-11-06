#!/usr/bin/env python3
"""Wspólny kod binarnego wykazu bezpieczeństwa VGKM.

Format zgodny z boot/crypto/manifest.c. Podpis obejmuje dokładne bajty VGKM.
"""
from __future__ import annotations

import hashlib
import re
import struct
import subprocess
from pathlib import Path, PurePosixPath

MAGIC = b"VGKM"
VERSION = 1
HEADER = struct.Struct("<4sHH")
ENTRY = struct.Struct("<HQ32s")
MAX_ENTRIES = 128
MAX_PATH = 240
MAX_SIZE = 65536
_ALLOWED = re.compile(r"^[A-Za-z0-9._/-]+$")


class VGKMError(Exception):
    """Błąd tworzenia lub sprawdzania wykazu VGKM."""


def validate_path(relative: str) -> bytes:
    if not isinstance(relative, str) or not relative or not _ALLOWED.fullmatch(relative):
        raise VGKMError(f"niedozwolona ścieżka: {relative!r}")
    pure = PurePosixPath(relative)
    if pure.is_absolute() or any(p in ("", ".", "..") for p in relative.split("/")):
        raise VGKMError(f"niedozwolona ścieżka: {relative}")
    encoded = relative.encode("ascii")
    if len(encoded) > MAX_PATH:
        raise VGKMError(f"ścieżka jest za długa: {relative}")
    return encoded


def checked_file(root: Path, relative: str) -> Path:
    validate_path(relative)
    current = root
    for part in relative.split("/"):
        current = current / part
        if current.is_symlink():
            raise VGKMError(f"ścieżka prowadzi przez dowiązanie: {relative}")
    try:
        resolved = current.resolve(strict=True)
        resolved.relative_to(root.resolve(strict=True))
    except (OSError, ValueError) as exc:
        raise VGKMError(f"brak pliku lub ścieżka poza wydaniem: {relative}") from exc
    if not resolved.is_file():
        raise VGKMError(f"to nie jest zwykły plik: {relative}")
    return resolved


def encode_manifest(root: Path, paths: list[str]) -> bytes:
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise VGKMError("katalog wydania nie istnieje")
    if not paths or len(paths) > MAX_ENTRIES:
        raise VGKMError("liczba plików musi wynosić od 1 do 128")
    if len(set(paths)) != len(paths):
        raise VGKMError("wykaz zawiera powtórzoną ścieżkę")
    records = []
    for relative in paths:
        encoded_path = validate_path(relative)
        file_path = checked_file(root, relative)
        digest = hashlib.sha256()
        size = 0
        with file_path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                size += len(chunk)
                digest.update(chunk)
        records.append((encoded_path, size, digest.digest()))
    records.sort(key=lambda item: item[0])
    raw = bytearray(HEADER.pack(MAGIC, VERSION, len(records)))
    for path, size, digest in records:
        raw.extend(ENTRY.pack(len(path), size, digest))
        raw.extend(path)
    if len(raw) > MAX_SIZE:
        raise VGKMError("binarny wykaz przekracza 65536 bajtów")
    return bytes(raw)


def decode_manifest(raw: bytes) -> list[tuple[str, int, bytes]]:
    if len(raw) > MAX_SIZE or len(raw) < HEADER.size:
        raise VGKMError("nieprawidłowy rozmiar wykazu")
    magic, version, count = HEADER.unpack_from(raw)
    if magic != MAGIC or version != VERSION or not 1 <= count <= MAX_ENTRIES:
        raise VGKMError("nieprawidłowy nagłówek VGKM")
    offset = HEADER.size
    records = []
    seen = set()
    for _ in range(count):
        if len(raw) - offset < ENTRY.size:
            raise VGKMError("ucięty wpis VGKM")
        path_len, size, digest = ENTRY.unpack_from(raw, offset)
        offset += ENTRY.size
        if not 1 <= path_len <= MAX_PATH or len(raw) - offset < path_len:
            raise VGKMError("nieprawidłowa długość ścieżki VGKM")
        try:
            path = raw[offset:offset + path_len].decode("ascii")
        except UnicodeDecodeError as exc:
            raise VGKMError("ścieżka VGKM nie jest tekstem ASCII") from exc
        validate_path(path)
        if path in seen:
            raise VGKMError(f"powtórzona ścieżka: {path}")
        seen.add(path)
        records.append((path, size, digest))
        offset += path_len
    if offset != len(raw):
        raise VGKMError("nadmiarowe dane na końcu VGKM")
    if [r[0].encode("ascii") for r in records] != sorted(r[0].encode("ascii") for r in records):
        raise VGKMError("wpisy VGKM nie są uporządkowane")
    return records


def sign_manifest(manifest: Path, private_key: Path, signature: Path) -> None:
    if signature.exists():
        raise VGKMError("plik podpisu już istnieje; nie zostanie nadpisany")
    result = subprocess.run(
        ["openssl", "pkeyutl", "-sign", "-inkey", str(private_key),
         "-rawin", "-in", str(manifest), "-out", str(signature)],
        capture_output=True, text=True, check=False,
    )
    if result.returncode or not signature.is_file() or signature.stat().st_size != 64:
        signature.unlink(missing_ok=True)
        raise VGKMError("nie udało się utworzyć 64-bajtowego podpisu Ed25519")


def verify_signature(manifest: Path, signature: Path, public_key: Path) -> None:
    if signature.stat().st_size != 64:
        raise VGKMError("podpis Ed25519 musi mieć dokładnie 64 bajty")
    result = subprocess.run(
        ["openssl", "pkeyutl", "-verify", "-pubin", "-inkey", str(public_key),
         "-sigfile", str(signature), "-rawin", "-in", str(manifest)],
        capture_output=True, text=True, check=False,
    )
    if result.returncode:
        raise VGKMError("podpis wykazu jest nieprawidłowy")


def verify_files(root: Path, records: list[tuple[str, int, bytes]]) -> None:
    root = root.resolve(strict=True)
    for relative, expected_size, expected_digest in records:
        path = checked_file(root, relative)
        digest = hashlib.sha256()
        size = 0
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                size += len(chunk)
                digest.update(chunk)
        if size != expected_size or digest.digest() != expected_digest:
            raise VGKMError(f"niezgodna zawartość lub rozmiar pliku: {relative}")
