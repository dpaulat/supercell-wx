@set script_dir=%~dp0

:: If conan_build_profile is not set, use the same as conan_profile
if "%conan_build_profile%" == "" (
    set conan_build_profile=%conan_profile%
)

@set cmake_args=-B "%build_dir%" -S "%script_dir%\..\.." ^
    -G "%generator%" ^
    -DCMAKE_PREFIX_PATH="%qt_base%/%qt_version%/%qt_arch%" ^
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="%script_dir%\..\..\external\cmake-conan\conan_provider.cmake" ^
    -DCONAN_HOST_PROFILE=%conan_profile% ^
    -DCONAN_BUILD_PROFILE=%conan_build_profile% ^
    -DSCWX_VIRTUAL_ENV=%venv_path% ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=on

:: Cross-builds (Windows ARM64) need x64 host tools for moc/rcc/uic.
:: scwx_config.cmake reads these from the environment before project().
@if defined qt_host_arch (
    set "QT_HOST_PATH=%qt_base%/%qt_version%/%qt_host_arch%"
    set "QT_ROOT_DIR=%qt_base%/%qt_version%/%qt_arch%"
    set cmake_args=%cmake_args% ^
        -DQT_HOST_PATH="%qt_base%/%qt_version%/%qt_host_arch%" ^
        -DQT_ROOT_DIR="%qt_base%/%qt_version%/%qt_arch%"
)

@if defined build_type (
    set cmake_args=%cmake_args% ^
        -DCMAKE_BUILD_TYPE=%build_type% ^
        -DCMAKE_CONFIGURATION_TYPES=%build_type% ^
        -DCONAN_INSTALL_BUILD_CONFIGURATIONS=%build_type%
) else (
    :: CMAKE_BUILD_TYPE isn't used to build, but is required by the Conan CMakeDeps generator
    set cmake_args=%cmake_args% ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_CONFIGURATION_TYPES=Debug;Release
)

@mkdir "%build_dir%"
cmake %cmake_args%
