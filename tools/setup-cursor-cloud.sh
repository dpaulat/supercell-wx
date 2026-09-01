#!/usr/bin/env bash
# Idempotent bootstrap for Cursor Cloud Agent environment builds.
# Recurring builds start from a clean Ubuntu image, so this script must
# provision python3-venv, Ninja, GCC as cc/c++, and Qt before running
# the normal Linux setup.
set -euo pipefail

script_dir="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
repo_root="$(readlink -f "${script_dir}/..")"
cd "${repo_root}"

export PATH="${HOME}/.local/bin:${PATH}"
export CC="${CC:-/usr/bin/gcc-13}"
export CXX="${CXX:-/usr/bin/g++-13}"

need_apt=0
if ! python3 -c "import ensurepip" 2>/dev/null; then
    need_apt=1
fi
if ! command -v ninja >/dev/null 2>&1; then
    need_apt=1
fi

if [ "${need_apt}" -eq 1 ]; then
    sudo apt-get update
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
        python3-venv \
        python3.12-venv \
        ninja-build \
        wayland-protocols \
        libwayland-dev \
        libwayland-egl-backend-dev
fi

if [ -x /usr/bin/gcc ] && [ -x /usr/bin/g++ ]; then
    sudo update-alternatives --set cc /usr/bin/gcc
    sudo update-alternatives --set c++ /usr/bin/g++
fi

qt_prefix="/opt/Qt/6.11.1/gcc_64"
if [ ! -d "${qt_prefix}" ]; then
    python3 -m pip install --user aqtinstall
    sudo mkdir -p /opt/Qt
    sudo chown "$(id -u):$(id -g)" /opt/Qt
    AQT_CONFIG="${repo_root}/tools/aqt-settings.ini" \
        aqt install-qt linux desktop 6.11.1 linux_gcc_64 \
        -m qtimageformats qtmultimedia qtpositioning qtserialport \
        --outputdir /opt/Qt
fi

git submodule update --init --recursive
"${script_dir}/setup-linux-ninja-release.sh"
