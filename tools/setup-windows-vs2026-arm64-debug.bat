@set script_dir=%~dp0

:: Configure variables for cross-compilation
@set build_dir=%script_dir%\..\build-debug-vs2026-arm64
@set build_type=Debug
@set conan_profile=scwx-windows_vs2026_armv8
@set conan_build_profile=scwx-windows_vs2026_x64
@set generator=Visual Studio 18 2026
@set vs_platform=ARM64
@set qt_base=C:/Qt
@set qt_arch=msvc2022_arm64
@set qt_host_arch=msvc2022_64
@set venv_path=%script_dir%\..\.venv

:: Assign user-specified build directory
@if not "%~1"=="" set build_dir=%~f1

:: Assign user-specified Python Virtual Environment
@if not "%~2"=="" (
    if /i "%~2"=="none" (
        set venv_path=
    ) else (
        set venv_path=%~f2
    )
)

:: Perform common setup
@call %script_dir%\lib\setup-common.bat

@pause
