#!/bin/bash
./tools/setup-common.sh

build_dir=${1:-build-release}
build_type=Release
qt_version=6.6.2
script_dir="$(dirname "$(readlink -f "$0")")"

mkdir -p ${build_dir}
cmake -B ${build_dir} -S . \
	-DCMAKE_BUILD_TYPE=${build_type} \
	-DCMAKE_CONFIGURATION_TYPES=${build_type} \
	-DCMAKE_INSTALL_PREFIX=${build_dir}/${build_type}/supercell-wx \
	-DCMAKE_PREFIX_PATH=/opt/Qt/${qt_version}/gcc_64 \
	-G Ninja
