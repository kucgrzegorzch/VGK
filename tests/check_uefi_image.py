#!/usr/bin/env python3
"""Structural checks for the VGK x86-64 UEFI PE32+ application."""

import struct
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"UEFI image check failed: {message}")


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: check_uefi_image.py path/to/BOOTX64.EFI")

    path = Path(sys.argv[1])
    data = path.read_bytes()

    if len(data) < 0x100:
        fail("file is too small to be a valid PE image")
    if data[:2] != b"MZ":
        fail("missing DOS MZ signature")

    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 24 > len(data):
        fail("PE header offset is outside the file")
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        fail("missing PE signature")

    coff = pe_offset + 4
    machine, section_count, _, _, _, optional_size, _ = struct.unpack_from(
        "<HHIIIHH", data, coff
    )
    if machine != 0x8664:
        fail(f"expected x86-64 machine 0x8664, got 0x{machine:04x}")
    if section_count == 0:
        fail("image has no sections")

    optional = coff + 20
    if optional + optional_size > len(data) or optional_size < 112:
        fail("invalid or truncated optional header")
    magic = struct.unpack_from("<H", data, optional)[0]
    if magic != 0x20B:
        fail(f"expected PE32+ optional header 0x20b, got 0x{magic:04x}")

    entry_rva = struct.unpack_from("<I", data, optional + 16)[0]
    image_size = struct.unpack_from("<I", data, optional + 56)[0]
    subsystem = struct.unpack_from("<H", data, optional + 68)[0]
    if entry_rva == 0:
        fail("entry point RVA is zero")
    if image_size == 0:
        fail("SizeOfImage is zero")
    if subsystem != 10:
        fail(f"expected EFI application subsystem 10, got {subsystem}")

    section_table = optional + optional_size
    entry_is_executable = False
    for index in range(section_count):
        offset = section_table + index * 40
        if offset + 40 > len(data):
            fail("truncated section table")
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
            "<IIII", data, offset + 8
        )
        characteristics = struct.unpack_from("<I", data, offset + 36)[0]
        span = max(virtual_size, raw_size)
        if virtual_address <= entry_rva < virtual_address + span:
            entry_is_executable = bool(characteristics & 0x20000000)
        if raw_size and raw_pointer + raw_size > len(data):
            fail("section data extends beyond end of file")

    if not entry_is_executable:
        fail("entry point is not inside an executable section")

    print(
        f"OK: {path} is PE32+ x86-64 EFI application "
        f"(entry RVA 0x{entry_rva:x}, {section_count} sections)"
    )


if __name__ == "__main__":
    main()
