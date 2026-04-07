#!/usr/bin/env bash
# Commit staged changes; fall back to git commit-tree when `git commit` fails
# (e.g. Cursor injects --trailer on Git < ~2.32).
set -euo pipefail

ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"

DO_PUSH=0
if [[ "${1:-}" == "--push" || "${1:-}" == "-p" ]]; then
	DO_PUSH=1
	shift
fi

if [[ $# -lt 1 ]]; then
	echo "usage: $0 [--push] <subject> [body line...]" >&2
	exit 1
fi

SUBJECT=$1
shift
MSGFILE=$(mktemp)
trap 'rm -f "$MSGFILE"' EXIT

if [[ $# -gt 0 ]]; then
	printf '%s\n\n' "$SUBJECT" > "$MSGFILE"
	printf '%s\n' "$@" >> "$MSGFILE"
else
	printf '%s\n' "$SUBJECT" > "$MSGFILE"
fi

if git diff --cached --quiet; then
	echo "error: nothing staged; run git add first" >&2
	exit 1
fi

if git commit -F "$MSGFILE" 2>/dev/null; then
	echo "Committed with git commit."
else
	TREE=$(git write-tree)
	PARENT=$(git rev-parse HEAD)
	COMMIT=$(git commit-tree "$TREE" -p "$PARENT" -F "$MSGFILE")
	git update-ref HEAD "$COMMIT"
	echo "Committed via commit-tree (git commit unsupported in this environment)."
fi

if [[ "$DO_PUSH" -eq 1 ]]; then
	git push
fi
