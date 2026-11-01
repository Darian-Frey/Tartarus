# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com). Reference F-, D-, AV-, BUG- and IMP- IDs for traceability. Upstream Descent 3 changes arrive via merges and are summarised, not re-listed; see upstream's CHANGELOG for detail.

## [Unreleased]
### Added
- `tools/upstream-sync.sh`: trial-merges `upstream/main` in a scratch worktree and fails on conflicts outside `Descent3/tartarus/` and the ARCHITECTURE.md hook-point list (IMP-002, F-001, AV-002) — 2026-11-01.
- Upstream Descent 3 merged in at `593b880f` (D-001, F-001); `upstream` remote added. Upstream README/BUILD/CHANGELOG preserved as `docs/UPSTREAM_*.md`; `.gitignore` merged with Tartarus block, exempting upstream's tracked HogMaker assets and test fixture (AV-005) — 2026-11-01.
- Project documentation set (README, FEATURES F-001–F-081, ROADMAP Phases 0–6, ARCHITECTURE, DECISIONS D-001–D-012, SPEC, ATTACK_VECTORS AV-001–AV-012, BUILD, BUGS, IMPROVEMENTS, CLAUDE) — 2026-11-01, against upstream `fa61eec`.
