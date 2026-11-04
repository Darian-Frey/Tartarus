#!/usr/bin/env python3
# Tartarus, a fork of Descent 3
# Copyright (C) 2026 Tartarus contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Classic Mode audit (AV-001).

Every upstream file with a TARTARUS marker, and every `tartarus::Name` or
`Tartarus_name` used from upstream code, must be listed in the hook table of
docs/CLASSIC_CHECKLIST.md with its Classic Mode behaviour. Exit status 1 lists
what is missing, so a new hook cannot land without a Classic review.

Usage: tools/classic_audit.py [--root DIR]
"""

import argparse
import pathlib
import re
import sys

SOURCE_SUFFIXES = {".cpp", ".h", ".txt", ".yml", ".cmake", ".glsl"}
SKIP_DIRS = {"builds", ".git", "third_party", "editor", "legacy"}
MARKER = re.compile(r"TARTARUS:")
SYMBOL = re.compile(r"\btartarus::([A-Za-z_][A-Za-z0-9_]*)|(?<!::)\b(Tartarus_[A-Za-z0-9_]+)")
# Namespaced helpers that are plumbing, not behaviour (types and flags).
IGNORED = {"Cvar", "Cvars", "CommandArgs", "CVAR_ARCHIVE", "CVAR_CLASSIC_LOCKED"}


def upstream_files(root: pathlib.Path):
    for p in root.rglob("*"):
        rel = p.relative_to(root)
        if not p.is_file() or p.suffix not in SOURCE_SUFFIXES:
            continue
        if rel.parts[0] in SKIP_DIRS or rel.parts[:2] == ("Descent3", "tartarus") or rel.parts[0] == "tools":
            continue
        yield rel, p


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", default=pathlib.Path(__file__).resolve().parent.parent, type=pathlib.Path)
    args = ap.parse_args()

    checklist = (args.root / "docs" / "CLASSIC_CHECKLIST.md").read_text(encoding="utf-8")
    table = "\n".join(l for l in checklist.splitlines() if l.startswith("|"))

    missing = []
    files = symbols = 0
    for rel, path in sorted(upstream_files(args.root)):
        text = path.read_text(encoding="utf-8", errors="replace")
        if not MARKER.search(text):
            continue
        files += 1
        if f"`{rel.as_posix()}`" not in table:
            missing.append(f"{rel.as_posix()}: hook file not in the checklist table")
        for m in SYMBOL.finditer(text):
            name = m.group(1) or m.group(2)
            if name in IGNORED:
                continue
            symbols += 1
            shown = f"tartarus::{name}" if m.group(1) else name
            # A bare name in a comment ("TARTARUS: Tartarus_Init") counts if listed with its namespace.
            if f"`{shown}`" not in table and f"`tartarus::{name}`" not in table:
                missing.append(f"{rel.as_posix()}: {shown} not in the checklist table")

    for line in sorted(set(missing)):
        print("missing:", line)
    print(f"{files} hook files, {symbols} uses checked, {len(set(missing))} missing")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
