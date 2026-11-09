#!/usr/bin/env python3
import subprocess
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: check_exception_stubs.py <kernel.elf>")

result = subprocess.run(
    ["nm", "-S", "-g", "--defined-only", sys.argv[1]],
    check=True, capture_output=True, text=True
)

symbols = {}
for line in result.stdout.splitlines():
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

print("OK: 32 exception stubs, fatal stop entry and 32-entry table linked")
