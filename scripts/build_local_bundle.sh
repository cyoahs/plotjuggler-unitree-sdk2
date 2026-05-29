#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
cd "${repo_root}"

jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
bundle_dir="${PJ_UNITREE_BUNDLE_DIRECTORY:-${PWD}/bundle/plotjuggler_unitree_sdk2}"
preset="${CMAKE_PRESET:-dev}"

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git submodule update --init --depth 1 \
    third_party/unitree_sdk2 \
    third_party/unitree_ros_assets
else
  if [[ ! -d third_party/unitree_sdk2 || ! -d third_party/unitree_ros_assets ]]; then
    echo "Not inside a valid Git worktree and required submodule directories are missing." >&2
    echo "Run this from a normal clone, or populate third_party/unitree_sdk2 and third_party/unitree_ros_assets manually." >&2
    exit 1
  fi
  echo "Skipping git submodule update because this is not a valid Git worktree." >&2
fi

plotjuggler_prefix="${PLOTJUGGLER_PREFIX:-}"
if [[ -z "${plotjuggler_prefix}" ]]; then
  export RUNNER_TEMP="${RUNNER_TEMP:-${PWD}/.deps}"
  plotjuggler_prefix="${PLOTJUGGLER_INSTALL_DIR:-${PWD}/.deps/plotjuggler-3.17.2}"
  export PLOTJUGGLER_INSTALL_DIR="${plotjuggler_prefix}"
  scripts/build_plotjuggler_dev.sh
fi
export PLOTJUGGLER_PREFIX="${plotjuggler_prefix}"

cmake --preset "${preset}" \
  -DPJ_UNITREE_BUNDLE_DIRECTORY="${bundle_dir}"

cmake --build --preset "${preset}" --parallel "${jobs}"
ctest --preset "${preset}"
