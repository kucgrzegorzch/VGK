#!/usr/bin/env python3
import re
import subprocess
import sys

ERROR_CODE_VECTORS = {8, 10, 11, 12, 13, 14, 17, 20, 21, 29, 30}

if len(sys.argv) != 2:
    raise SystemExit("usage: check_exception_stubs.py <kernel.elf>")

elf = sys.argv[1]
nm = subprocess.run(
    ["nm", "-S", "-g", "--defined-only", elf],
    check=True, capture_output=True, text=True
)

symbols = {}
for line in nm.stdout.splitlines():
    fields = line.split()
    if len(fields) >= 4:
        try:
            symbols[fields[3]] = (int(fields[1], 16), fields[2])
        except ValueError:
            continue

required = [f"vgk_exception_stub_{i}" for i in range(32)]
required += ["vgk_exception_fatal_entry", "vgk_exception_stub_table"]
missing = [name for name in required if name not in symbols]
if missing:
    raise SystemExit("FAIL: missing exception symbols: " + ", ".join(missing))

table_size, table_type = symbols["vgk_exception_stub_table"]
if table_size != 32 * 8 or table_type.upper() != "R":
    raise SystemExit(
        f"FAIL: exception stub table must be 256-byte read-only data; "
        f"size={table_size}, type={table_type}"
    )

for i in range(32):
    if symbols[f"vgk_exception_stub_{i}"][1].upper() != "T":
        raise SystemExit(f"FAIL: vector {i} stub is not in executable text")

disassembly = subprocess.run(
    ["objdump", "-d", "--no-show-raw-insn", elf],
    check=True, capture_output=True, text=True
)

blocks = {}
current = None
for line in disassembly.stdout.splitlines():
    label = re.match(r"^[0-9a-fA-F]+ <([^>]+)>:$", line.strip())
    if label:
        current = label.group(1)
        blocks[current] = []
        continue
    if current is not None:
        instruction = re.match(
            r"^\s*[0-9a-fA-F]+:\s*([a-z][a-z0-9.]*)\s*(.*?)\s*$",
            line
        )
        if instruction:
            blocks[current].append(
                (instruction.group(1), instruction.group(2))
            )

for vector in range(32):
    name = f"vgk_exception_stub_{vector}"
    instructions = blocks.get(name, [])
    expected_pushes = [vector] if vector in ERROR_CODE_VECTORS else [0, vector]
    expected_mnemonics = ["push"] * len(expected_pushes) + ["jmp"]

    if [mnemonic for mnemonic, _ in instructions] != expected_mnemonics:
        raise SystemExit(
            f"FAIL: {name} instruction sequence mismatch: {instructions}"
        )

    for index, expected_value in enumerate(expected_pushes):
        operand = instructions[index][1]
        match = re.fullmatch(r"\$(0x[0-9a-fA-F]+|[0-9]+)", operand)
        if not match or int(match.group(1), 0) != expected_value:
            raise SystemExit(
                f"FAIL: {name} push {index} must place {expected_value}; "
                f"found {operand!r}"
            )

    if "vgk_exception_fatal_entry" not in instructions[-1][1]:
        raise SystemExit(f"FAIL: {name} does not jump to fatal entry")

fatal = blocks.get("vgk_exception_fatal_entry", [])
fatal_mnemonics = [mnemonic for mnemonic, _ in fatal]
if (
    len(fatal_mnemonics) < 3
    or fatal_mnemonics[0] != "cli"
    or "hlt" not in fatal_mnemonics[1:]
    or fatal_mnemonics[-1] != "jmp"
):
    raise SystemExit(
        "FAIL: fatal entry must begin with CLI, contain HLT, and end with JMP; "
        f"found {fatal}"
    )

print(
    "OK: 32 exception stubs have correct vector/error-code pushes; "
    "fatal entry disables IRQs and halts"
)
