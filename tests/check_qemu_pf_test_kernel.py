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

sized_symbols = subprocess.run(
    ["nm", "-S", "--defined-only", path],
    check=True, capture_output=True, text=True
).stdout
symbol_sizes = {}
for line in sized_symbols.splitlines():
    fields = line.split()
    if len(fields) >= 4:
        symbol_sizes[fields[3]] = (int(fields[1], 16), fields[2])

if symbol_sizes.get("vgk_kernel_idt") != (4096, "b") or \
   symbol_sizes.get("vgk_kernel_idtr", (None, None))[0] != 10:
    raise SystemExit("FAIL: QEMU image must contain a 4096-byte full IDT and 10-byte IDTR")
for symbol in ("vgk_kernel_entry", "vgk_exception_stub_14",
               "vgk_exception_fatal_entry", "vgk_exception_stub_table",
               "vgk_qemu_page_fault_observe", "vgk_ist_page_fault_stack_get_bounds",
               "vgk_qemu_double_fault_observe", "vgk_double_fault_test_stub",
               "vgk_double_fault_capture_entry", "vgk_ist_double_fault_stack_get_bounds"):
    if symbol not in defined_names:
        raise SystemExit(f"FAIL: missing required test symbol: {symbol}")

for target in ("vgk_qemu_page_fault_observe", "vgk_ist_page_fault_stack_get_bounds",
               "vgk_qemu_double_fault_observe", "vgk_ist_double_fault_stack_get_bounds"):
    if not any("call" in line and f"<{target}>" in line
               for line in disassembly.splitlines()):
        raise SystemExit(f"FAIL: missing call to {target}")

print("OK: isolated QEMU image contains full 256-entry IDT and links #PF/#DF observers; IST range checks are called; LGDT/LTR/LIDT present; CR3 unchanged")
