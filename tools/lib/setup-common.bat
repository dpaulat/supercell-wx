@set script_dir=%~dp0

:: Import common paths
@call "%script_dir%common-paths.bat"

:: Initialize MSVC environment
@call "%script_dir%setup-msvc.bat"

:: Activate Python Virtual Environment
@if defined venv_path (
    echo Activating Python Virtual Environment: %venv_path%
    py -3 -m venv "%venv_path%"
    call "%venv_path%\Scripts\activate.bat"
)

:: Install Python packages
py -3 -m pip install --upgrade pip
pip install --upgrade -r "%script_dir%\..\..\requirements.txt"

@if defined build_type (
    :: Install Conan profile and packages
    call "%script_dir%setup-conan.bat"
) else (
    :: Install Conan profile and debug packages
    set build_type=Debug
    call "%script_dir%setup-conan.bat"

    :: Install Conan profile and release packages
    set build_type=Release
    call "%script_dir%setup-conan.bat"

    :: Unset build_type
    set build_type=
)

:: Run CMake Configure
@call "%script_dir%run-cmake-configure.bat"

:: Deactivate Python Virtual Environment
@if defined venv_path (
    call "%venv_path%\Scripts\deactivate.bat"
)
