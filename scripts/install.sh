#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build/install"
install_prefix="${WVM_INSTALL_PREFIX:-/usr/local}"

cmake -S "${project_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWVM_BUILD_GUI=ON \
    -DBUILD_TESTING=ON
cmake --build "${build_dir}" --parallel
ctest --test-dir "${build_dir}" --output-on-failure

if [[ -w "${install_prefix}" || -w "$(dirname -- "${install_prefix}")" ]]; then
    cmake --install "${build_dir}" --prefix "${install_prefix}"
else
    sudo cmake --install "${build_dir}" --prefix "${install_prefix}"
fi

echo "Installed WVM to ${install_prefix}"
