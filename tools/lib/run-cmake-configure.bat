@set script_dir=%~dp0

@set cmake_args=-B "%build_dir%" -S "%script_dir%\..\.." ^
    -G "%generator%" ^
    -DCMAKE_PREFIX_PATH="%qt_base%/%qt_version%/%qt_arch%" ^
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="%script_dir%\..\..\external\cmake-conan\conan_provider.cmake" ^
    -DCONAN_HOST_PROFILE=%conan_profile% ^
    -DCONAN_BUILD_PROFILE=%conan_profile% ^
    -DSCWX_VIRTUAL_ENV="%venv_path%" ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=on

@if defined build_type (
    set cmake_args=%cmake_args% ^
        -DCMAKE_BUILD_TYPE=%build_type% ^
        -DCMAKE_CONFIGURATION_TYPES=%build_type%
) else (
    :: CMAKE_BUILD_TYPE isn't used to build, but is required by the Conan CMakeDeps generator
    set cmake_args=%cmake_args% ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_CONFIGURATION_TYPES=Debug;Release
)

@mkdir "%build_dir%"
cmake %cmake_args%
