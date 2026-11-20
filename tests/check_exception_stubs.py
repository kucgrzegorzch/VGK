#!/usr/bin/env python3
import re
import subprocess
import sys

ERROR_CODE_VECTORS = {8, 10, 11, 12, 13, 14, 17, 21, 29, 30}
SAVED_REGISTERS = [
    "rax", "rbx", "rcx", "rdx", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
]

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
required += [
    "vgk_exception_fatal_entry", "vgk_exception_stub_table",
    "vgk_vector_stub_table", "vgk_double_fault_test_stub",
    "vgk_double_fault_capture_entry",
]
required += [f"vgk_interrupt_stub_{i}" for i in range(32, 256)]
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

vector_table_size, vector_table_type = symbols["vgk_vector_stub_table"]
if vector_table_size != 256 * 8 or vector_table_type.upper() != "R":
    raise SystemExit(
        f"FAIL: complete vector stub table must be 2048-byte read-only data; "
        f"size={vector_table_size}, type={vector_table_type}"
    )

for vector in range(32, 256):
    if symbols[f"vgk_interrupt_stub_{vector}"][1].upper() != "T":
        raise SystemExit(f"FAIL: vector {vector} stub is not in executable text")

for name in ("vgk_double_fault_test_stub", "vgk_double_fault_capture_entry"):
    if symbols[name][1].upper() != "T":
        raise SystemExit(f"FAIL: {name} is not in executable text")

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

    if vector == 14:
        expected_pf = [
            ("push", "%rax"),
            ("mov", "%cr2,%rax"),
            ("xchg", "%rax,(%rsp)"),
            ("push", "$0xe"),
        ]
        if instructions[:4] != expected_pf or len(instructions) != 5 or \
           instructions[4][0] != "jmp" or \
           "vgk_exception_fatal_entry" not in instructions[4][1]:
            raise SystemExit(
                "FAIL: page-fault stub must preserve RAX, capture CR2, "
                "then push vector 14 and jump to fatal entry; "
                f"found {instructions}"
            )
        continue

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

# Every vector 32..255 has a no-error-code fail-stop stub. It pushes
# a synthetic zero error code and its vector before entering the common path.
for vector in range(32, 256):
    name = f"vgk_interrupt_stub_{vector}"
    instructions = blocks.get(name, [])
    expected = [("push", "$0x0"), ("push", f"$0x{vector:x}")]
    if (
        len(instructions) != 3
        or instructions[:2] != expected
        or instructions[2][0] != "jmp"
        or "vgk_exception_fatal_entry" not in instructions[2][1]
    ):
        raise SystemExit(
            f"FAIL: {name} must push synthetic error code/vector and jump "
            f"to fatal entry; found {instructions}"
        )

# The isolated #DF stub is intentionally separate from the live IDT table.
df_stub = blocks.get("vgk_double_fault_test_stub", [])
if (
    len(df_stub) != 2
    or df_stub[0] != ("push", "$0x8")
    or df_stub[1][0] != "jmp"
    or "vgk_double_fault_capture_entry" not in df_stub[1][1]
):
    raise SystemExit(
        "FAIL: isolated #DF stub must push vector 8 and jump to its own entry; "
        f"found {df_stub}"
    )

df_entry = blocks.get("vgk_double_fault_capture_entry", [])
df_mnemonics = [mnemonic for mnemonic, _ in df_entry]
df_operands = [operand.replace("%", "").strip() for _, operand in df_entry]
df_expected_save = ["push " + register for register in SAVED_REGISTERS]
df_actual_save = [
    f"{mnemonic} {operand}"
    for mnemonic, operand in zip(df_mnemonics, df_operands)
][1:1 + len(SAVED_REGISTERS)]
if (
    not df_mnemonics
    or df_mnemonics[0] != "cli"
    or df_actual_save != df_expected_save
    or "hlt" not in df_mnemonics
    or not any(
        mnemonic == "jmp" and "vgk_double_fault_capture_entry+" in operand
        for mnemonic, operand in df_entry
    )
):
    raise SystemExit(
        "FAIL: #DF capture entry must CLI, save all 15 GPRs in order, then halt; "
        f"found {df_entry}"
    )

fatal = blocks.get("vgk_exception_fatal_entry", [])
fatal_mnemonics = [mnemonic for mnemonic, _ in fatal]
fatal_operands = [operand.replace("%", "").strip() for _, operand in fatal]
expected_save = [f"push {register}" for register in SAVED_REGISTERS]
actual_save = [
    f"{mnemonic} {operand}" for mnemonic, operand in zip(fatal_mnemonics, fatal_operands)
][1:1 + len(SAVED_REGISTERS)]
hlt_positions = [
    index for index, mnemonic in enumerate(fatal_mnemonics)
    if mnemonic == "hlt"
]
backward_loop = any(
    mnemonic == "jmp"
    and "vgk_exception_fatal_entry+" in operand
    for mnemonic, operand in fatal
)
if (
    not fatal_mnemonics
    or fatal_mnemonics[0] != "cli"
    or actual_save != expected_save
    or not hlt_positions
    or not any(
        any(mnemonic == "jmp" for mnemonic, _ in fatal[index + 1:])
        for index in hlt_positions
    )
    or not backward_loop
):
    raise SystemExit(
        "FAIL: fatal entry must CLI, save all 15 GPRs in order, then halt; "
        f"found {fatal}"
    )

print(
    "OK: 32 exception stubs and 224 fail-stop vector stubs normalize vector/error code; isolated #DF stub "
    "uses its own 22-qword capture entry; both fatal paths save all 15 GPRs"
)
