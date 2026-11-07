#!/usr/bin/env python3
import struct
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: check_native_kernel.py <kernel.elf>")

path = sys.argv[1]
data = open(path, "rb").read()

if len(data) < 64 or data[:4] != b"\x7fELF":
    raise SystemExit("FAIL: missing ELF header")
if data[4] != 2 or data[5] != 1 or data[6] != 1:
    raise SystemExit("FAIL: expected ELF64 little-endian")
e_type, e_machine = struct.unpack_from("<HH", data, 16)
if e_type != 2 or e_machine != 62:
    raise SystemExit("FAIL: expected x86-64 executable ELF")
entry = struct.unpack_from("<Q", data, 24)[0]
phoff = struct.unpack_from("<Q", data, 32)[0]
ehsize, phentsize, phnum = struct.unpack_from("<HHH", data, 52)[0], struct.unpack_from("<H", data, 54)[0], struct.unpack_from("<H", data, 56)[0]
if ehsize != 64 or phentsize != 56 or phnum < 1:
    raise SystemExit("FAIL: invalid ELF program header table")
if not (0x200000 <= entry < 0x40000000):
    raise SystemExit(f"FAIL: entry outside expected low physical range: 0x{entry:x}")

loads = []
for index in range(phnum):
    off = phoff + index * phentsize
    p_type, p_flags = struct.unpack_from("<II", data, off)
    p_offset, p_vaddr, p_paddr, p_filesz, p_memsz = struct.unpack_from("<QQQQQ", data, off + 8)
    if p_type == 1:
        if p_filesz > p_memsz or p_offset + p_filesz > len(data):
            raise SystemExit("FAIL: invalid PT_LOAD bounds")
        loads.append((p_paddr, p_memsz, p_flags))

if not loads:
    raise SystemExit("FAIL: no PT_LOAD segments")
if not any(start <= entry < start + size and (flags & 1) for start, size, flags in loads):
    raise SystemExit("FAIL: entry is not in an executable PT_LOAD segment")
if any(flags & 2 and flags & 1 for _, _, flags in loads):
    raise SystemExit("FAIL: writable executable segment detected")

print(f"OK: native VGK ELF64 x86-64; entry=0x{entry:x}; PT_LOAD={len(loads)}")
