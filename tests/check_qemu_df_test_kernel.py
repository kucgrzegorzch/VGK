#!/usr/bin/env python3
import re
import subprocess
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: check_qemu_df_test_kernel.py <kernel.elf>")

path = sys.argv[1]
header = open(path, "rb").read(64)
if len(header) < 64 or header[:4] != b"\x7fELF":
    raise SystemExit("FAIL: missing ELF header")
if header[4] != 2 or header[5] != 1 or header[6] != 1:
    raise SystemExit("FAIL: expected ELF64 little-endian")
import struct
e_type, e_machine = struct.unpack_from("<HH", header, 16)
if e_type != 2 or e_machine != 62:
    raise SystemExit("FAIL: expected x86-64 executable ELF")

disassembly = subprocess.run(
    ["objdump", "-d", "--no-show-raw-insn", path],
    check=True, capture_output=True, text=True
).stdout.lower()
for instruction in ("lgdt", "ltr", "lidt"):
    if instruction not in disassembly:
        raise SystemExit(f"FAIL: test image is missing {instruction}")
if any("mov" in line and "%cr3" in line for line in disassembly.splitlines()):
    raise SystemExit("FAIL: double-fault test must not activate a new CR3")

symbols_text = subprocess.run(
    ["nm", "--defined-only", path], check=True, capture_output=True, text=True
).stdout
symbols = {line.split()[-1] for line in symbols_text.splitlines() if line.split()}

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
    raise SystemExit("FAIL: #DF image must contain a 4096-byte full IDT and 10-byte IDTR")
required = (
    "vgk_kernel_entry", "vgk_exception_stub_14", "vgk_exception_stub_8",
    "vgk_exception_stub_table", "vgk_double_fault_test_stub",
    "vgk_double_fault_capture_entry", "vgk_qemu_double_fault_observe",
    "vgk_ist_double_fault_stack_get_bounds", "vgk_qemu_trigger_double_fault",
)
missing = [name for name in required if name not in symbols]
if missing:
    raise SystemExit("FAIL: missing required #DF test symbols: " + ", ".join(missing))

blocks = {}
current = None
for line in disassembly.splitlines():
    match = re.match(r"^[0-9a-f]+ <([^>]+)>:$", line.strip())
    if match:
        current = match.group(1)
        blocks[current] = []
    elif current is not None:
        blocks[current].append(line)

df_entry = "\n".join(blocks.get("vgk_double_fault_capture_entry", []))
if "call" not in df_entry or "vgk_qemu_double_fault_observe" not in df_entry:
    raise SystemExit("FAIL: #DF capture entry does not call its observer")
trigger = "\n".join(blocks.get("vgk_qemu_trigger_double_fault", []))
for instruction in ("lgdt", "ltr", "lidt"):
    if instruction not in trigger:
        raise SystemExit(f"FAIL: controlled #DF trigger is missing {instruction}")

strings = subprocess.run(
    ["strings", path], check=True, capture_output=True, text=True
).stdout
for marker in (
    "QEMU #PF gate intentionally not present",
    "QEMU test IDT loaded; #PF gate absent; triggering #DF.",
    "QEMU DOUBLE FAULT IST1 PASS",
    "QEMU DOUBLE FAULT CAPTURE PASS",
    "QEMU full 256-entry IDT descriptor PASS",
):
    if marker not in strings:
        raise SystemExit("FAIL: missing diagnostic marker: " + marker)

print("OK: isolated #DF image contains a dedicated IST1 capture path, observer, "
      "controlled trigger and diagnostics; LGDT/LTR/LIDT present; CR3 unchanged")
