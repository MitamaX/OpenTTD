#!/usr/bin/env bash
# Usage: worktree.sh <lane> <from> [branch]   creates the lane worktree on branch lane/<lane> (or [branch]) and builds it.
set -euo pipefail

if (( $# < 2 )); then
	echo "usage: $0 <lane> <from> [branch]" >&2
	exit 2
fi

verify_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
main_repo=$(cd "$verify_dir/.." && pwd)
lane=$1
from=$2
branch=${3:-lane/$lane}
worktree="${OTTD_WORKTREE_ROOT:-/home/user/ottd-wt}/$lane"

if [[ ! -e "$worktree/.git" ]]; then
	if git -C "$main_repo" show-ref --verify --quiet "refs/heads/$branch"; then
		git -C "$main_repo" worktree add "$worktree" "$branch"
	else
		git -C "$main_repo" worktree add -b "$branch" "$worktree" "$from"
	fi
fi

"$verify_dir/build.sh" "$worktree" >&2
echo "$worktree/build/openttd"
