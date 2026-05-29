#!/usr/bin/env bash
set -euo pipefail

ref="${PLOTJUGGLER_REF:-3.17.2}"
jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
root="${RUNNER_TEMP:-/tmp}/plotjuggler-dev"
src_dir="${PLOTJUGGLER_SOURCE_DIR:-${root}/src}"
build_dir="${PLOTJUGGLER_BUILD_DIR:-${root}/build}"
install_dir="${PLOTJUGGLER_INSTALL_DIR:-${root}/install}"

if [[ ! -d "${src_dir}/.git" ]]; then
  rm -rf "${src_dir}"
  git clone --depth 1 --branch "${ref}" \
    https://github.com/PlotJuggler/PlotJuggler.git "${src_dir}"
fi

cmake -S "${src_dir}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${install_dir}" \
  -DBASE_AS_SHARED=ON \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

cmake --build "${build_dir}" --target install --parallel "${jobs}"

printf '%s\n' "${install_dir}"
