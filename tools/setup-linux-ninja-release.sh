#!/bin/bash
set -euo pipefail
script_dir="$(dirname "$(readlink -f "$0")")"

export PATH="${HOME}/.local/bin:${PATH}"
export CC="${CC:-/usr/bin/gcc-13}"
export CXX="${CXX:-/usr/bin/g++-13}"

export build_dir="$(readlink -f "${1:-${script_dir}/../build-release}")"
export build_type=Release
export conan_profile=${2:-scwx-linux_gcc-13}
export generator=Ninja
export qt_base=/opt/Qt
export qt_arch=gcc_64
export address_sanitizer=${4:-disabled}

# Assign user-specified Python Virtual Environment
[ "${3:-}" = "none" ] && unset venv_path || export venv_path="$(readlink -f "${3:-${script_dir}/../.venv}")"

# Perform common setup
"${script_dir}/lib/setup-common.sh"
