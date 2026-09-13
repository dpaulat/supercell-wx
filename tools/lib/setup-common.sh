#!/bin/bash
set -euo pipefail

script_source="${BASH_SOURCE[0]:-$0}"
script_dir="$(cd "$(dirname "${script_source}")" && pwd)"

# User-site scripts (conan, aqt, pip) must be on PATH even when a venv
# is not active. Environment-build install environments often omit this.
export PATH="${HOME}/.local/bin:${PATH}"

# Import common paths
source "${script_dir}/common-paths.sh"

# Load custom build settings
if [ -f "${script_dir}/user-setup.sh" ]; then
    source "${script_dir}/user-setup.sh"
fi

# Activate python3 Virtual Environment
if [ -n "${venv_path:-}" ]; then
    if ! python3 -c "import ensurepip" 2>/dev/null; then
        echo "error: python3 venv/ensurepip is not available." >&2
        echo "On Debian/Ubuntu, install it with: sudo apt-get install -y python3-venv" >&2
        exit 1
    fi
    # Recreate a leftover tree from a previous failed venv (no pip/activate)
    if [ -d "${venv_path}" ] && ! "${venv_path}/bin/python" -c "import pip" 2>/dev/null; then
        rm -rf "${venv_path}"
    fi
    python3 -m venv "${venv_path}"
    # shellcheck disable=SC1091
    source "${venv_path}/bin/activate"
fi

# Detect if a python3 Virtual Environment was specified above, or elsewhere
IN_VENV=$(python3 -c 'import sys; print(sys.prefix != getattr(sys, "base_prefix", sys.prefix))')

if [ "${IN_VENV}" = "True" ]; then
    # In a virtual environment, don't use --user
    PIP_FLAGS="--upgrade"
else
    # Not in a virtual environment, use --user
    PIP_FLAGS="--upgrade --user"
fi

# Install python3 packages
# shellcheck disable=SC2086
python3 -m pip install ${PIP_FLAGS} pip
# shellcheck disable=SC2086
python3 -m pip install ${PIP_FLAGS} -r "${script_dir}/../../requirements.txt"

if [[ -n "${build_type:-}" ]]; then
    # Install Conan profile and packages
    "${script_dir}/setup-conan.sh"
else
    # Install Conan profile and debug packages
    export build_type=Debug
    "${script_dir}/setup-conan.sh"

    # Install Conan profile and release packages
    export build_type=Release
    "${script_dir}/setup-conan.sh"

    # Unset build_type
    unset build_type
fi

# Run CMake Configure
"${script_dir}/run-cmake-configure.sh"

# Deactivate python3 Virtual Environment
if [ -n "${venv_path:-}" ]; then
    deactivate
fi
