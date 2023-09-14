call tools\setup-common.bat
mkdir build-release
cd build-release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.5.2/msvc2019_64 ..
pause
