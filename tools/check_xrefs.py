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

"""Cross-reference checker for the Tartarus standard documents (IMP-001).

Walks the project's Markdown documents, collects every stable ID defined by a
heading (`### F-012 ...`, `### BUG-004: ...`) and every ID mentioned anywhere.

Errors (exit status 1):
  - an ID is mentioned but defined nowhere;
  - an ID is defined twice.
Warnings (exit status 0 unless --strict):
  - one-way links: entry A lists B on a `**Related...**` line, but B's entry
    never mentions A.

Usage: tools/check_xrefs.py [--strict] [--root DIR]
"""

import argparse
import pathlib
import re
import sys

PREFIXES = ("F", "D", "AV", "BUG", "IMP")
ID_RE = re.compile(r"\b(?:%s)-\d{3}\b" % "|".join(PREFIXES))
HEADING_RE = re.compile(r"^#{2,4}\s+((?:%s)-\d{3})\b" % "|".join(PREFIXES))
RELATED_RE = re.compile(r"^\*\*Related[^*]*\*\*")


def documents(root: pathlib.Path):
    docs = sorted(root.glob("*.md")) + sorted((root / "docs").glob("*.md"))
    # Upstream's own docs and machine-local notes are not ours to check.
    return [p for p in docs if not p.name.startswith("UPSTREAM_") and p.name != "CLAUDE.local.md"]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--strict", action="store_true", help="treat warnings as errors")
    ap.add_argument("--root", default=pathlib.Path(__file__).resolve().parent.parent, type=pathlib.Path)
    args = ap.parse_args()

    defined = {}     # id -> (file, line)
    bodies = {}      # id -> text of its entry (heading to next heading of the same or higher level)
    related = {}     # id -> set of ids on its Related lines
    mentions = []    # (id, file, line)
    errors, warnings = [], []

    for path in documents(args.root):
        lines = path.read_text(encoding="utf-8").splitlines()
        current = None
        for n, line in enumerate(lines, 1):
            m = HEADING_RE.match(line)
            if m:
                ident = m.group(1)
                if ident in defined:
                    f, l = defined[ident]
                    errors.append(f"{path.name}:{n}: {ident} defined again (first at {f}:{l})")
                else:
                    defined[ident] = (path.name, n)
                current = ident
                bodies.setdefault(ident, [])
                related.setdefault(ident, set())
                continue
            if line.startswith("## ") or line.startswith("# ") or line.startswith("---"):
                current = None  # a section break ends the entry
            if current is not None:
                bodies[current].append(line)
                if RELATED_RE.match(line):
                    related[current].update(i for i in ID_RE.findall(line) if i != current)
            for ident in ID_RE.findall(line):
                mentions.append((ident, path.name, n))

    for ident, f, n in mentions:
        if ident not in defined:
            errors.append(f"{f}:{n}: {ident} is referenced but not defined")

    for a, targets in sorted(related.items()):
        for b in sorted(targets):
            if b in bodies and not re.search(r"\b%s\b" % re.escape(a), "\n".join(bodies[b])):
                f, n = defined[a]
                warnings.append(f"{f}:{n}: {a} lists {b} as related, but {b} ({defined[b][0]}) does not mention {a}")

    for e in errors:
        print("error:", e)
    for w in warnings:
        print("warning:", w)
    print(f"{len(defined)} IDs defined, {len(mentions)} mentions, {len(errors)} errors, {len(warnings)} warnings")
    return 1 if errors or (args.strict and warnings) else 0


if __name__ == "__main__":
    sys.exit(main())
