call tools\setup-common.bat

set build_dir=build-release
set build_type=Release
set qt_version=6.8.0

mkdir %build_dir%
cmake -B %build_dir% -S . ^
    -DCMAKE_BUILD_TYPE=%build_type% ^
    -DCMAKE_CONFIGURATION_TYPES=%build_type% ^
    -DCMAKE_PREFIX_PATH=C:/Qt/%qt_version%/msvc2019_64
pause
