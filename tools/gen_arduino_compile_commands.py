#!/usr/bin/env python3
"""
Generate compile_commands.json for the ESP32 sketches, with the real board flags.

Why this exists
---------------
`esp32/master/master.ino` and `esp32/sniffer/sniffer.ino` are built by
arduino-cli, which knows the exact include path for the selected board - the
Arduino core, the esp32s3 variant, the bundled libraries and ~340 ESP-IDF
component directories. VS Code knows none of it, so every `#include <WiFi.h>`
and `#include <esp_now.h>` comes up unresolved.

arduino-cli can emit a compilation database, but three things stop VS Code from
using it as-is:

1. Arduino compiles a GENERATED file. The database names
   `<build>/sketch/master.ino.cpp`, not `esp32/master/master.ino`, so cpptools
   never matches the file you actually have open. The entry is retargeted here.
2. The command line is mostly gcc `@response` files - `flags/includes` alone
   holds 336 `-iwithprefixbefore` entries. cpptools does not expand `@file`.
   They are expanded here.
3. Those entries are relative to a separate `-iprefix`. They are resolved to
   plain absolute `-I` here, which cpptools does understand.

What is left is a database that needs neither arduino-cli nor the build
directory to stay around.

This is the exact answer for this repo. The machine-wide fallback for sketches
outside it is the shim built by ~/.local/bin/refresh-intellisense-shims.py,
wired into C_Cpp.default.includePath - that one resolves names but knows nothing
about which board is selected.

Usage
-----
    python3 tools/gen_arduino_compile_commands.py            # all esp32/ sketches
    python3 tools/gen_arduino_compile_commands.py --check    # verify, then write
    python3 tools/gen_arduino_compile_commands.py --fqbn esp32:esp32:esp32

Re-run after changing board, adding a sketch, or upgrading the core.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SKETCH_ROOT = REPO / "esp32"

# ESP32-S3 Dev Module - the board esp32/README.md builds and uploads to.
DEFAULT_FQBN = "esp32:esp32:esp32s3"

# Build-directory bookkeeping that means nothing to an editor.
DROP_EXACT = {"-MMD", "-c", "-w"}


def sketches() -> list[Path]:
    return sorted(p for p in SKETCH_ROOT.glob("*/") if (p / f"{p.name}.ino").is_file())


def expand_response_files(args: list[str]) -> list[str]:
    """Splice @file arguments in place - cpptools does not read them itself."""
    out: list[str] = []
    for arg in args:
        if not arg.startswith("@"):
            out.append(arg)
            continue
        path = Path(arg[1:])
        if not path.is_file():
            continue
        out += expand_response_files(path.read_text().split())
    return out


def absolutise_includes(args: list[str]) -> list[str]:
    """Turn -iprefix/-iwithprefixbefore pairs into plain -I<abs>.

    The ESP32 core passes one -iprefix and then names every component directory
    relative to it. cpptools handles -I reliably and the prefixed forms much less
    so, and the pair is trivial to resolve here.
    """
    out: list[str] = []
    prefix = ""
    index = 0
    while index < len(args):
        arg = args[index]
        if arg == "-iprefix":
            prefix = args[index + 1]
            index += 2
        elif arg.startswith("-iprefix"):
            prefix = arg[len("-iprefix"):]
            index += 1
        elif arg in {"-iwithprefixbefore", "-iwithprefix"}:
            out.append(f"-I{prefix}{args[index + 1]}")
            index += 2
        else:
            out.append(arg)
            index += 1
    return out


def clean(args: list[str], build: Path) -> list[str]:
    """Strip the compile step's own plumbing, keep the flags that describe the code."""
    out: list[str] = []
    index = 0
    while index < len(args):
        arg = args[index]
        if arg == "-o":
            index += 2
            continue
        if arg in DROP_EXACT:
            index += 1
            continue
        # the generated .ino.cpp and anything else living in the build directory
        if str(build) in arg:
            index += 1
            continue
        out.append(arg)
        index += 1
    return out


def entry_for(sketch: Path, fqbn: str) -> dict | None:
    ino = sketch / f"{sketch.name}.ino"
    build = Path(tempfile.mkdtemp(prefix=f"ino-{sketch.name}-"))
    try:
        result = subprocess.run(
            ["arduino-cli", "compile", "--fqbn", fqbn,
             "--only-compilation-database", "--build-path", str(build), str(sketch)],
            capture_output=True, text=True,
        )
        database = build / "compile_commands.json"
        if not database.is_file():
            print(f"FAIL {ino.name}: arduino-cli produced no database", file=sys.stderr)
            print(result.stderr.strip()[:2000], file=sys.stderr)
            return None

        generated = [e for e in json.loads(database.read_text())
                     if e["file"].endswith(f"{sketch.name}.ino.cpp")]
        if not generated:
            print(f"FAIL {ino.name}: no entry for the sketch itself", file=sys.stderr)
            return None

        args = clean(absolutise_includes(expand_response_files(generated[0]["arguments"])), build)
        args.append(str(ino))
        return {"directory": str(sketch), "file": str(ino), "arguments": args}
    finally:
        shutil.rmtree(build, ignore_errors=True)


def check(entries: list[dict]) -> int:
    """Compile each sketch with the flags we are about to hand VS Code.

    A raw .ino is not valid C++ on its own - Arduino injects the function
    prototypes that let setup() call something defined further down - so only
    unresolved includes are treated as failures here. That is the one thing the
    database is responsible for.
    """
    failures = 0
    for entry in entries:
        args = entry["arguments"]
        result = subprocess.run(
            [args[0], "-fsyntax-only", "-x", "c++", *args[1:]],
            capture_output=True, text=True, cwd=entry["directory"],
        )
        missing = [line for line in result.stderr.splitlines() if "No such file" in line]
        name = Path(entry["file"]).name
        if missing:
            failures += len(missing)
            print(f"FAIL {name}: {len(missing)} unresolved include(s)", file=sys.stderr)
            for line in missing[:5]:
                print("   ", line.strip(), file=sys.stderr)
        else:
            includes = sum(1 for a in args if a.startswith("-I"))
            print(f"OK   {name}: every include resolves ({includes} include paths)")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fqbn", default=DEFAULT_FQBN)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("-o", "--output", default=str(REPO / "compile_commands.json"))
    args = parser.parse_args()

    if not shutil.which("arduino-cli"):
        print("arduino-cli not on PATH", file=sys.stderr)
        return 1

    found = sketches()
    if not found:
        print(f"no sketches under {SKETCH_ROOT}", file=sys.stderr)
        return 1

    entries = [e for e in (entry_for(s, args.fqbn) for s in found) if e]
    if len(entries) != len(found):
        return 1

    if args.check and check(entries):
        return 1

    Path(args.output).write_text(json.dumps(entries, indent=2) + "\n")
    print(f"wrote {args.output} ({len(entries)} sketches, fqbn {args.fqbn})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
