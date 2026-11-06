#!/usr/bin/env python3
"""Generuje klucz wydawcy VGK i nagłówek z samym kluczem publicznym."""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ED25519_SPKI_PREFIX = bytes.fromhex("302a300506032b6570032100")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Utwórz parę kluczy Ed25519 dla wydań VGK.")
    parser.add_argument("--private-key", required=True, type=Path,
                        help="miejsce na tajny klucz PEM; zachowaj poza repozytorium")
    parser.add_argument("--public-key", required=True, type=Path,
                        help="miejsce na jawny klucz PEM")
    parser.add_argument("--header", required=True, type=Path,
                        help="miejsce na nagłówek C zawierający wyłącznie klucz jawny")
    args = parser.parse_args()

    outputs = [args.private_key, args.public_key, args.header]
    resolved = [p.absolute() for p in outputs]
    if len(set(resolved)) != len(resolved):
        parser.error("każdy wynik musi mieć inną ścieżkę")
    for path in outputs:
        if path.exists() or path.is_symlink():
            parser.error(f"plik już istnieje; nic nie zostanie nadpisane: {path}")
        if not path.parent.is_dir():
            parser.error(f"katalog docelowy nie istnieje: {path.parent}")

    old_umask = os.umask(0o077)
    der_path = None
    try:
        subprocess.run(
            ["openssl", "genpkey", "-algorithm", "Ed25519",
             "-out", str(args.private_key)],
            check=True, capture_output=True)
        os.chmod(args.private_key, 0o600)
        subprocess.run(
            ["openssl", "pkey", "-in", str(args.private_key),
             "-pubout", "-out", str(args.public_key)],
            check=True, capture_output=True)

        # Twórz plik tymczasowy z unikalną nazwą i wyłącznym utworzeniem.
        # Nie używaj przewidywalnej nazwy obok nagłówka, bo mogłaby wskazywać
        # na istniejący plik lub dowiązanie symboliczne.
        fd, temporary_name = tempfile.mkstemp(
            prefix=".vgk-public-", suffix=".der", dir=str(args.header.parent))
        os.close(fd)
        der_path = Path(temporary_name)
        subprocess.run(
            ["openssl", "pkey", "-pubin", "-in", str(args.public_key),
             "-outform", "DER", "-out", str(der_path)],
            check=True, capture_output=True)
        der = der_path.read_bytes()

        if len(der) != len(ED25519_SPKI_PREFIX) + 32 or not der.startswith(ED25519_SPKI_PREFIX):
            raise RuntimeError("OpenSSL zwrócił nieoczekiwany format klucza Ed25519")
        raw = der[len(ED25519_SPKI_PREFIX):]
        values = ", ".join(f"0x{byte:02x}" for byte in raw)
        header = (
            "#ifndef VGK_TRUSTED_PUBLIC_KEY_H\n"
            "#define VGK_TRUSTED_PUBLIC_KEY_H\n\n"
            "/* Jawny klucz wydawcy VGK. Nie zawiera klucza prywatnego. */\n"
            "#include <stdint.h>\n"
            "#define VGK_TRUSTED_PUBLIC_KEY_INITIALIZER { " + values + " }\n\n"
            "#endif\n"
        )
        with args.header.open("x", encoding="ascii") as output:
            output.write(header)
    except (OSError, subprocess.CalledProcessError, RuntimeError) as exc:
        for path in outputs:
            path.unlink(missing_ok=True)
        print(f"BŁĄD tworzenia klucza: {exc}", file=sys.stderr)
        return 1
    finally:
        if der_path is not None:
            der_path.unlink(missing_ok=True)
        os.umask(old_umask)

    print("Utworzono klucz wydawcy VGK.")
    print(f"Klucz prywatny: {args.private_key} (uprawnienia 600)")
    print(f"Klucz publiczny: {args.public_key}")
    print(f"Nagłówek z kluczem publicznym: {args.header}")
    print("Nie wysyłaj ani nie umieszczaj klucza prywatnego w repozytorium, ISO lub artefaktach.")
    print("Zachowaj kopię klucza prywatnego poza komputerem roboczym, w bezpiecznym miejscu offline.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
