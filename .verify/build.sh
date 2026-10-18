#!/usr/bin/env bash
# Usage: build.sh [source_dir]   configures and builds <source_dir>/build, the repository itself by default.
set -euo pipefail

verify_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source_dir=$(cd "${1:-$verify_dir/..}" && pwd)
build_dir="$source_dir/build"
# Shared root for relative ccache paths, so every worktree under it hits the same cache entries.
ccache_base_dir=${OTTD_CCACHE_BASEDIR:-/home/user}
jobs=${OTTD_BUILD_JOBS:-$(nproc)}

configure_definitions=(-DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPTION_USE_ASSERTS=ON)
if command -v ccache > /dev/null; then
	launcher="env;CCACHE_BASEDIR=$ccache_base_dir;CCACHE_NOHASHDIR=true;ccache"
	# ccache cannot cache units built against a precompiled header.
	configure_definitions+=("-DCMAKE_C_COMPILER_LAUNCHER=$launcher" "-DCMAKE_CXX_COMPILER_LAUNCHER=$launcher" -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON)
fi

generator=()
if [[ ! -f "$build_dir/CMakeCache.txt" ]] && command -v ninja > /dev/null; then
	generator=(-G Ninja)
fi
cmake -S "$source_dir" -B "$build_dir" "${generator[@]}" "${configure_definitions[@]}"

started=$SECONDS
cmake --build "$build_dir" -j "$jobs"
echo "built in $((SECONDS - started))s: $build_dir/openttd"
