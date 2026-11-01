#!/usr/bin/env bash
# tools/upstream-sync.sh — trial-merge upstream Descent3 and report the conflict set.
#
# Implements IMP-002 for F-001 / AV-002. By default it does a dry run: the
# merge is attempted in a throwaway worktree, so your checkout is never touched.
#
#   tools/upstream-sync.sh            dry run against upstream/main
#   tools/upstream-sync.sh --apply    merge for real into the current branch
#                                     (--no-commit, so you review before committing)
#   UPSTREAM_REF=upstream/foo tools/upstream-sync.sh   merge a different ref
#
# A conflict is "allowed" when it lies in Descent3/tartarus/, in a file listed in
# ARCHITECTURE.md §Hook points (parsed from the table, not duplicated here), or in
# one of the root files Tartarus deliberately replaces (README/BUILD/CHANGELOG,
# .gitignore). Any other conflict means the fork has drifted: exit status 1.
#
# Exit: 0 clean or allowed conflicts only; 1 conflicts outside the allowed set;
#       2 usage / environment error.

set -euo pipefail

REMOTE=upstream
REF=${UPSTREAM_REF:-upstream/main}
APPLY=0

case "${1:-}" in
  "") ;;
  --apply) APPLY=1 ;;
  -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
  *) echo "unknown argument: $1 (try --help)" >&2; exit 2 ;;
esac

ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"

git remote get-url "$REMOTE" >/dev/null 2>&1 || {
  echo "no '$REMOTE' remote; add it with:" >&2
  echo "  git remote add upstream https://github.com/DescentDevelopers/Descent3" >&2
  exit 2
}

# --- allowed conflict set -------------------------------------------------
# Every backticked token in column 1 of the hook-point table that looks like a
# path to a source/build file. Tokens without a '/' are reported, because the
# table must use full paths for this to be reliable.
mapfile -t HOOK_FILES < <(
  sed -n '/^## Hook points/,/^## /p' ARCHITECTURE.md |
    grep '^| `' |
    sed -E 's/^\| ([^|]*)\|.*/\1/' |
    grep -oE '`[^`]+`' | tr -d '`' |
    sed -E 's/::.*//; s/ .*//' |
    grep -E '\.(c|cpp|h|hpp|txt|cmake)$' | sort -u
)
if [ "${#HOOK_FILES[@]}" -eq 0 ]; then
  echo "could not parse any hook-point files from ARCHITECTURE.md" >&2
  exit 2
fi
for f in "${HOOK_FILES[@]}"; do
  [[ $f == */* ]] || echo "warning: hook-point entry '$f' is not a full path; fix ARCHITECTURE.md" >&2
done
ROOT_REPLACED=(README.md BUILD.md CHANGELOG.md .gitignore)

is_allowed() {
  local p=$1 f
  [[ $p == Descent3/tartarus/* ]] && return 0
  for f in "${HOOK_FILES[@]}" "${ROOT_REPLACED[@]}"; do [[ $p == "$f" ]] && return 0; done
  return 1
}

# --- fetch -----------------------------------------------------------------
echo "fetching $REMOTE ..."
git fetch --quiet "$REMOTE"
git rev-parse --verify --quiet "$REF^{commit}" >/dev/null || { echo "unknown ref: $REF" >&2; exit 2; }

BASE=$(git merge-base HEAD "$REF" || true)
NEW=$(git rev-list --count "HEAD..$REF")
echo "upstream: $REF = $(git rev-parse --short "$REF"), $NEW commit(s) not yet merged"
echo "markers:  $(git grep -I -c 'TARTARUS:' -- ':!*.md' | awk -F: '{s+=$2} END {print s+0}') '// TARTARUS:' markers outside docs (log per release, AV-002)"
if [ "$NEW" -eq 0 ]; then echo "already up to date."; exit 0; fi
[ -n "$BASE" ] || echo "note: no merge base with $REF (unrelated histories?)" >&2

# --- merge -------------------------------------------------------------------
if [ "$APPLY" -eq 1 ]; then
  if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "working tree has uncommitted changes; commit or stash first" >&2
    exit 2
  fi
  WT=$ROOT
else
  WT=$(mktemp -d "${TMPDIR:-/tmp}/tartarus-sync.XXXXXX")
  trap 'git -C "$ROOT" worktree remove --force "$WT" >/dev/null 2>&1 || true; rm -rf "$WT"' EXIT
  git worktree add --quiet --detach "$WT" HEAD
fi

set +e
git -C "$WT" merge --no-ff --no-commit "$REF" >/dev/null 2>&1
set -e
mapfile -t CONFLICTS < <(git -C "$WT" diff --name-only --diff-filter=U)

BAD=0
if [ "${#CONFLICTS[@]}" -eq 0 ]; then
  echo "result:   merges cleanly"
else
  echo "result:   ${#CONFLICTS[@]} conflicting file(s):"
  for p in "${CONFLICTS[@]}"; do
    if is_allowed "$p"; then echo "  allowed  $p"; else echo "  DRIFT    $p"; BAD=1; fi
  done
fi

if [ "$APPLY" -eq 1 ]; then
  echo
  echo "merge left uncommitted in your checkout. Resolve conflicts (if upstream changed"
  echo "README/BUILD/CHANGELOG, refresh docs/UPSTREAM_*.md from $REF), then 'git commit'."
  echo "'git merge --abort' backs out."
else
  git -C "$WT" merge --abort >/dev/null 2>&1 || true
  echo "dry run only; re-run with --apply to merge."
fi

if [ "$BAD" -eq 1 ]; then
  echo "conflicts outside Descent3/tartarus/ and the hook-point list: the fork has drifted (AV-002)." >&2
  exit 1
fi
exit 0
