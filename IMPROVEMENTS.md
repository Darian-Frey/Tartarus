# Improvements

Catalogue of code-quality improvements, refactors, and architectural
changes proposed during development. Per Maintenance Rule 8, improvements
are logged here when noticed, not silently applied. The author decides
whether to apply, defer, or decline.

This is the dual of BUGS.md: bugs are broken; improvements work but
could be better.

Status vocabulary: suggested | applied | declined | deferred.
Effort vocabulary: trivial | small | medium | large.

## Suggested

### IMP-001: Cross-reference checker for the standard documents
**Status:** suggested
**Found:** 2026-11-01 (documentation scaffolding session)
**Location:** `tools/check_xrefs.py` (new)
**Effort:** small
**Description.** The docs already cross-link F-/D-/AV- IDs in both directions by hand; Maintenance Rule 6 says this decays without tooling.
**Proposal.** Script walks all `*.md`, builds the ID graph, reports IDs referenced but undefined and references that are unidirectional. Run as a pre-commit hook and in CI.
**Trade-offs.** One more script to maintain; false positives on prose mentions of IDs unless the parser is restricted to `**Related:**` lines and tables.
**Notes.** Phase 1 deliverable per D-011.

## Applied

### IMP-002: Upstream-sync helper
**Status:** applied
**Found:** 2026-11-01
**Location:** `tools/upstream-sync.sh` (new)
**Effort:** small
**Description.** Monthly upstream merges are the main maintenance cost of the fork strategy (D-001, AV-002).
**Proposal.** Fetch `upstream`, attempt merge on a scratch branch, list conflicting files, fail if any lie outside `Descent3/tartarus/` or the ARCHITECTURE.md hook-point list, print the `// TARTARUS:` marker count.
**Trade-offs.** Encodes the hook-point list twice (script and doc) unless the script parses ARCHITECTURE.md's table — do that.
**Notes.** Phase 0 deliverable. Applied 2026-11-01 as `tools/upstream-sync.sh`: dry run in a throwaway worktree by default, `--apply` merges with `--no-commit`; the hook-point list is parsed from ARCHITECTURE.md, plus `Descent3/tartarus/` and the root files Tartarus replaces (README/BUILD/CHANGELOG/.gitignore).


## Declined

## Deferred
