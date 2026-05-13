@echo off

set "script_dir=%~dp0"
set "build_dir=%script_dir%..\build\windows-vs2026-x64-ninja-release"
set "target=supercell-wx"
set "config=Release"

:: Initialize MSVC environment
call "%script_dir%lib\setup-msvc.bat"

if %ERRORLEVEL% neq 0 (
    echo Error: Failed to initialize MSVC environment.
    exit /b %ERRORLEVEL%
)

:: Check if build files exist
if not exist "%build_dir%\build.ninja" (
    echo Build files missing or incomplete in %build_dir%. Running setup first...
    call "%script_dir%setup-windows-ninja-release.bat"
)

:: Run the build
echo.
echo ==============================================================================
echo Building %target% (%config%) in %build_dir%
echo ==============================================================================
cmake --build "%build_dir%" --target %target% --config %config%

if %ERRORLEVEL% neq 0 (
    echo.
    echo Build failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo.
echo Build completed successfully.
pause
