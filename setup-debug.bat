call tools\setup-common.bat

set build_dir=build-debug
set build_type=Debug
set conan_profile=scwx-win64_msvc2022
set qt_version=6.9.0
set qt_arch=msvc2022_64

conan config install tools/conan/profiles/%conan_profile% -tf profiles
conan install . ^
    --remote conancenter ^
    --build missing ^
    --profile:all %conan_profile% ^
    --settings:all build_type=%build_type% ^
    --output-folder %build_dir%/conan

mkdir %build_dir%
cmake -B %build_dir% -S . ^
    -DCMAKE_BUILD_TYPE=%build_type% ^
    -DCMAKE_CONFIGURATION_TYPES=%build_type% ^
    -DCMAKE_PREFIX_PATH=C:/Qt/%qt_version%/%qt_arch% ^
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=external/cmake-conan/conan_provider.cmake ^
    -DCONAN_HOST_PROFILE=%conan_profile% ^
    -DCONAN_BUILD_PROFILE=%conan_profile%
pause
