#!/bin/bash
./tools/setup-common.sh
mkdir -p build-release
cd build-release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_PREFIX_PATH=/opt/Qt/6.6.0/gcc_64 ..
