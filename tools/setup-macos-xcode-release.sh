#!/bin/bash
script_source="${BASH_SOURCE[0]:-$0}"
script_dir="$(cd "$(dirname "${script_source}")" && pwd)"

export build_dir="$(python3 -c 'import os,sys;print(os.path.realpath(sys.argv[1]))' "${1:-${script_dir}/../build-xcode-release}")"
export build_type=Release
export conan_profile=${2:-scwx-macos_clang-22_armv8}
export generator=Xcode
export qt_base="/Users/${USER}/Qt"
export qt_arch=macos
export address_sanitizer=${4:-disabled}

# Set explicit compiler paths
if ! llvm_prefix="$(brew --prefix llvm@22)"; then
    echo "error: failed to resolve Homebrew prefix for llvm@22" >&2
    exit 1
fi
export CC="${llvm_prefix}/bin/clang"
export CXX="${llvm_prefix}/bin/clang++"
export PATH="${llvm_prefix}/bin:$PATH"

export LDFLAGS="-L${llvm_prefix}/lib -L${llvm_prefix}/lib/c++"
export CPPFLAGS="-I${llvm_prefix}/include"

# Clang 21+ requires macOS 13.3
export MACOSX_DEPLOYMENT_TARGET=13.3

# Assign user-specified Python Virtual Environment
if [ "${3:-}" = "none" ]; then
    unset venv_path
else
    # macOS does not have 'readlink -f', use python for realpath
    export venv_path="$(python3 -c 'import os,sys;print(os.path.realpath(sys.argv[1]))' "${3:-${script_dir}/../.venv}")"
fi

# FIXME: aws-sdk-cpp fails to configure using Xcode
echo "Xcode is not supported"
read -p "Press Enter to continue..."

# Perform common setup
# "${script_dir}/lib/setup-common.sh"
