#!/bin/bash
./tools/setup-common.sh
build_dir=build-debug
build_type=Debug
qt_version=6.6.1
script_dir="$(dirname "$(readlink -f "$0")")"
mkdir -p ${build_dir}
cmake -B ${build_dir} -S . -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_CONFIGURATION_TYPES=${build_type} -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=${script_dir}/external/cmake-conan/conan_provider.cmake -DCMAKE_PREFIX_PATH=/opt/Qt/${qt_version}/gcc_64 -G Ninja
