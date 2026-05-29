#!/usr/bin/env bash
set -euo pipefail

version="${1:-dev}"
bundle_dir="${2:-bundle/plotjuggler_unitree_sdk2}"
dist_dir="${3:-dist}"
platform="${4:-linux-x86_64}"

if [[ ! -d "${bundle_dir}" ]]; then
  echo "Bundle directory does not exist: ${bundle_dir}" >&2
  exit 1
fi

package_name="plotjuggler-unitree-sdk2-${version}-${platform}"
package_dir="${dist_dir}/${package_name}"

rm -rf "${package_dir}"
mkdir -p "${package_dir}" "${dist_dir}"
cp -a "${bundle_dir}/." "${package_dir}/"

tarball="${dist_dir}/${package_name}.tar.gz"
rm -f "${tarball}" "${tarball}.sha256"
tar -C "${dist_dir}" -czf "${tarball}" "${package_name}"
sha256sum "${tarball}" > "${tarball}.sha256"

printf '%s\n' "${tarball}"
