#!/usr/bin/env python3
import re
import struct
import subprocess
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
ehsize = struct.unpack_from("<H", data, 52)[0]
phentsize = struct.unpack_from("<H", data, 54)[0]
phnum = struct.unpack_from("<H", data, 56)[0]
if ehsize != 64 or phentsize != 56 or phnum < 1:
    raise SystemExit("FAIL: invalid ELF program header table")
if phoff + phentsize * phnum > len(data):
    raise SystemExit("FAIL: program header table exceeds file bounds")
if not (0x200000 <= entry < 0x40000000):
    raise SystemExit(f"FAIL: entry outside expected low physical range: 0x{entry:x}")

loads = []
load_segments = []
for index in range(phnum):
    off = phoff + index * phentsize
    p_type, p_flags = struct.unpack_from("<II", data, off)
    p_offset, p_vaddr, p_paddr, p_filesz, p_memsz = struct.unpack_from("<QQQQQ", data, off + 8)
    if p_type == 1:
        if p_filesz > p_memsz or p_offset + p_filesz > len(data):
            raise SystemExit("FAIL: invalid PT_LOAD bounds")
        if p_vaddr != p_paddr or p_memsz == 0:
            raise SystemExit("FAIL: expected identity-mapped, non-empty load segment")
        if p_paddr + p_memsz > 0x40000000:
            raise SystemExit("FAIL: load segment exceeds the initial low-memory window")
        if (p_flags & 4) == 0 or (p_flags & ~7) != 0:
            raise SystemExit("FAIL: PT_LOAD must use known ELF R/W/X permissions")
        loads.append((p_paddr, p_memsz, p_flags))
        load_segments.append((p_paddr, p_filesz, p_memsz, p_flags))

if not loads:
    raise SystemExit("FAIL: no PT_LOAD segments")
if not any(start <= entry < start + size and (flags & 1) for start, size, flags in loads):
    raise SystemExit("FAIL: entry is not in an executable PT_LOAD segment")
if any(flags & 2 and flags & 1 for _, _, flags in loads):
    raise SystemExit("FAIL: writable executable segment detected")

# The emergency IST stacks live in .bss.ist. Verify that the linker keeps
# them inside the RW, non-executable PT_LOAD zero-fill tail: the UEFI ELF
# loader allocates p_memsz and explicitly zeroes the bytes after p_filesz.
required_ist_stacks = {
    "vgk_ist_double_fault_stack",
    "vgk_ist_nmi_stack",
    "vgk_ist_machine_check_stack",
    "vgk_ist_page_fault_stack",
}
nm_output = subprocess.run(
    ["nm", "-S", "--defined-only", path],
    check=True, capture_output=True, text=True
).stdout
symbols = {}
for line in nm_output.splitlines():
    fields = line.split()
    if len(fields) >= 4 and fields[3] in required_ist_stacks:
        try:
            symbols[fields[3]] = (int(fields[0], 16), int(fields[1], 16), fields[2])
        except ValueError:
            raise SystemExit("FAIL: malformed IST stack symbol from nm")

if set(symbols) != required_ist_stacks:
    missing = sorted(required_ist_stacks - set(symbols))
    raise SystemExit("FAIL: missing IST stack symbols: " + ", ".join(missing))

for name, (address, size, symbol_type) in symbols.items():
    if symbol_type.lower() != "b" or size != 16 * 1024:
        raise SystemExit(f"FAIL: {name} must be a 16 KiB BSS object")
    if address % 4096 != 0:
        raise SystemExit(f"FAIL: {name} is not 4 KiB aligned")
    end = address + size
    in_zero_filled_rw_segment = any(
        (flags & 6) == 6 and (flags & 1) == 0 and
        address >= start + file_size and end <= start + memory_size
        for start, file_size, memory_size, flags in load_segments
    )
    if not in_zero_filled_rw_segment:
        raise SystemExit(
            f"FAIL: {name} is not wholly inside an RW, NX PT_LOAD BSS tail"
        )
if not any((flags & 7) == 4 for _, _, flags in loads):
    raise SystemExit("FAIL: expected a dedicated read-only, non-executable PT_LOAD segment")

disassembly = subprocess.run(
    ["objdump", "-d", path],
    check=True, capture_output=True, text=True
).stdout
forbidden_instruction = re.compile(
    r"\b(?:lgdt|ltr|lidt)\b|\bmov[a-z]*\s+[^,\n]+,\s*%cr3\b",
    re.IGNORECASE,
)
for line in disassembly.splitlines():
    if forbidden_instruction.search(line):
        raise SystemExit(
            "FAIL: privileged descriptor/CR3 activation is not permitted yet: "
            + line.strip()
        )

# The page-fault stub must preserve interrupted RAX and capture CR2 before
# adding the vector number or performing the common GPR-save sequence.
pf_disassembly = subprocess.run(
    ["objdump", "-d", "--disassemble=vgk_exception_stub_14", path],
    check=True, capture_output=True, text=True
).stdout
required_pf_order = [
    r"\\bpush\\s+%rax\\b",
    r"\\bmov\\s+%cr2\\s*,\\s*%rax\\b",
    r"\\bxchg\\s+%rax\\s*,\\s*\\(%rsp\\)",
    r"\\bpush\\s+\\$0xe\\b",
    r"\\bjmp\\s+.*<vgk_exception_fatal_entry>",
]
positions = []
for pattern in required_pf_order:
    match = re.search(pattern, pf_disassembly, re.IGNORECASE)
    if match is None:
        raise SystemExit("FAIL: page-fault early CR2 capture sequence is incomplete: " + pattern)
    positions.append(match.start())
if positions != sorted(positions):
    raise SystemExit("FAIL: page-fault stub does not capture CR2 before vector/common entry")


print(f"OK: native VGK ELF64 x86-64; entry=0x{entry:x}; PT_LOAD={len(loads)}; RX/R/RW separated; LGDT/LTR/LIDT/CR3 writes absent")
