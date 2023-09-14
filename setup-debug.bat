call tools\setup-common.bat
mkdir build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CONFIGURATION_TYPES=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.5.2/msvc2019_64 ..
pause
