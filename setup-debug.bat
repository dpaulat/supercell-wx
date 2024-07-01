call tools\setup-common.bat

set build_dir=build-debug
set build_type=Debug
set qt_version=6.7.2

mkdir %build_dir%
cmake -B %build_dir% -S . ^
    -DCMAKE_BUILD_TYPE=%build_type% ^
    -DCMAKE_CONFIGURATION_TYPES=%build_type% ^
    -DCMAKE_PREFIX_PATH=C:/Qt/%qt_version%/msvc2019_64
pause
