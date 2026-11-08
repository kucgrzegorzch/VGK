#!/usr/bin/env python3
"""Create a C trust-anchor header from an ephemeral Ed25519 SubjectPublicKeyInfo DER."""
import sys
from pathlib import Path

PREFIX = bytes.fromhex("302a300506032b6570032100")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: generate_test_trust_header.py PUBLIC.der OUTPUT.h", file=sys.stderr)
        return 2
    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    raw = source.read_bytes()
    if len(raw) != len(PREFIX) + 32 or not raw.startswith(PREFIX):
        print("invalid Ed25519 SubjectPublicKeyInfo DER", file=sys.stderr)
        return 1
    key = raw[len(PREFIX):]
    values = ", ".join(f"0x{byte:02x}" for byte in key)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "#ifndef VGK_TRUSTED_PUBLIC_KEY_H\\n"
        "#define VGK_TRUSTED_PUBLIC_KEY_H\\n"
        "/* Ephemeral CI-only key. Never use for a published VGK release. */\\n"
        f"#define VGK_TRUSTED_PUBLIC_KEY_INITIALIZER {{ {values} }}\\n"
        "#endif\\n",
        encoding="ascii",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
