@setlocal enabledelayedexpansion

@set script_dir=%~dp0
@set venv_path=%script_dir%\..\.venv

:: Assign user-specified Python Virtual Environment
@if not "%~1"=="" (
    if /i "%~1"=="none" (
        set venv_path=
    ) else (
        set venv_path=%~f1
    )
)

:: Activate Python Virtual Environment
@if defined venv_path (
    echo Activating Python Virtual Environment: %venv_path%
    python -m venv %venv_path%
    call %venv_path%\Scripts\activate.bat
)

:: Install Python packages
python -m pip install --upgrade pip
pip install --upgrade -r "%script_dir%\..\requirements.txt"

:: Configure default Conan profile
@conan profile detect -e

:: Conan profiles
@set profile_count=2
@set /a last_profile=profile_count - 1
@set conan_profile[0]=scwx-windows_vs2022_x64
@set conan_profile[1]=scwx-windows_vs2026_x64

:: Install Conan profiles
@for /L %%i in (0,1,!last_profile!) do @(
    :: Install the base profile
    set "profile_name=!conan_profile[%%i]!"
    set "profile_path=%script_dir%\conan\profiles\!profile_name!"
    conan config install "!profile_path!" -tf profiles

    :: Create debug profile in temp directory
    set "debug_profile_name=!profile_name!-debug"
    set "debug_profile_path=%TEMP%\!debug_profile_name!"
    copy "!profile_path!" "!debug_profile_path!" >nul

    :: Replace build_type=Release with build_type=Debug
    powershell -Command "(Get-Content '!debug_profile_path!') -replace 'build_type=Release', 'build_type=Debug' | Set-Content '!debug_profile_path!'"

    :: Install the debug profile
    conan config install "!debug_profile_path!" -tf profiles

    :: Remove temporary debug profile
    del "!debug_profile_path!"
)

:: Deactivate Python Virtual Environment
@if defined venv_path (
    call %venv_path%\Scripts\deactivate.bat
)

@pause
