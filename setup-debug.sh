#!/bin/bash
./tools/setup-common.sh
mkdir -p build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CONFIGURATION_TYPES=Debug -DCMAKE_PREFIX_PATH=/opt/Qt/6.6.1/gcc_64 ..
