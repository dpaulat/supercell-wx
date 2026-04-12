#!/bin/bash
script_dir="$(dirname "$(readlink -f "$0")")"

export build_dir="$(readlink -f "${1:-${script_dir}/../build-multi}")"
export conan_profile=${2:-scwx-linux_gcc-13}
export generator="Ninja Multi-Config"
export qt_base=/opt/Qt
export qt_arch=gcc_64
export address_sanitizer=${4:-disabled}

# Assign user-specified Python Virtual Environment
[ "${3:-}" = "none" ] && unset venv_path || export venv_path="$(readlink -f "${3:-${script_dir}/../.venv}")"

# FIXME: aws-sdk-cpp fails to configure using Ninja Multi-Config
echo "Ninja Multi-Config is not supported in Linux"
read -p "Press Enter to continue..."

# Perform common setup
# "${script_dir}/lib/setup-common.sh"
