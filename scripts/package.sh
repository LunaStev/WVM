#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build/package"

cmake -S "${project_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWVM_BUILD_GUI=ON \
    -DBUILD_TESTING=ON
cmake --build "${build_dir}" --parallel
ctest --test-dir "${build_dir}" --output-on-failure

generators=(TGZ)
if command -v dpkg-deb >/dev/null 2>&1; then
    generators+=(DEB)
fi
if command -v rpmbuild >/dev/null 2>&1; then
    generators+=(RPM)
fi

for generator in "${generators[@]}"; do
    cpack --config "${build_dir}/CPackConfig.cmake" -G "${generator}"
done

echo "Packages are available in ${build_dir}"
