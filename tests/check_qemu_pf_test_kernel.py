#!/usr/bin/env python3
import struct
import subprocess
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: check_qemu_pf_test_kernel.py <kernel.elf>")

path = sys.argv[1]
data = open(path, "rb").read()
if len(data) < 64 or data[:4] != b"\x7fELF":
    raise SystemExit("FAIL: missing ELF header")
if data[4] != 2 or data[5] != 1 or data[6] != 1:
    raise SystemExit("FAIL: expected ELF64 little-endian")
e_type, e_machine = struct.unpack_from("<HH", data, 16)
if e_type != 2 or e_machine != 62:
    raise SystemExit("FAIL: expected x86-64 executable ELF")

disassembly = subprocess.run(
    ["objdump", "-d", path], check=True, capture_output=True, text=True
).stdout.lower()
for instruction in ("lgdt", "ltr", "lidt"):
    if instruction not in disassembly:
        raise SystemExit(f"FAIL: QEMU test image is missing {instruction}")
if any("mov" in line and "%cr3" in line for line in disassembly.splitlines()):
    raise SystemExit("FAIL: QEMU page-fault test must not activate a new CR3")

symbols = subprocess.run(
    ["nm", "--defined-only", path], check=True, capture_output=True, text=True
).stdout
defined_names = {line.split()[-1] for line in symbols.splitlines() if line.split()}
for symbol in ("vgk_kernel_entry", "vgk_exception_stub_14",
               "vgk_exception_fatal_entry", "vgk_exception_stub_table",
               "vgk_qemu_page_fault_observe"):
    if symbol not in defined_names:
        raise SystemExit(f"FAIL: missing required test symbol: {symbol}")

if not any("call" in line and "<vgk_qemu_page_fault_observe>" in line
           for line in disassembly.splitlines()):
    raise SystemExit("FAIL: #PF stub does not call the C context observer")

print("OK: isolated QEMU #PF test ELF; C context observer linked and called; LGDT/LTR/LIDT present; CR3 unchanged")
