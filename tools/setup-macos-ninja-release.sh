#!/bin/bash
script_source="${BASH_SOURCE[0]:-$0}"
script_dir="$(cd "$(dirname "${script_source}")" && pwd)"

export build_dir="$(python3 -c 'import os,sys;print(os.path.realpath(sys.argv[1]))' "${1:-${script_dir}/../build-release}")"
export build_type=Release
export conan_profile=${2:-scwx-macos_clang-22_armv8}
export generator=Ninja
export qt_base="/Users/${USER}/Qt"
export qt_arch=macos
export address_sanitizer=${4:-disabled}

# Set explicit compiler paths
export CC=$(brew --prefix llvm@22)/bin/clang
export CXX=$(brew --prefix llvm@22)/bin/clang++
export PATH="$(brew --prefix llvm@22)/bin:$PATH"

export LDFLAGS="-L$(brew --prefix llvm@22)/lib -L$(brew --prefix llvm@22)/lib/c++"
export CPPFLAGS="-I$(brew --prefix llvm@22)/include"

# Assign user-specified Python Virtual Environment
if [ "${3:-}" = "none" ]; then
    unset venv_path
else
    # macOS does not have 'readlink -f', use python for realpath
    export venv_path="$(python3 -c 'import os,sys;print(os.path.realpath(sys.argv[1]))' "${3:-${script_dir}/../.venv}")"
fi

# Perform common setup
"${script_dir}/lib/setup-common.sh"
