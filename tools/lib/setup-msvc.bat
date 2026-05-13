@echo off

:: Check if cl.exe is already in the path
where cl.exe >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo MSVC environment already initialized.
    exit /b 0
)

echo Initializing MSVC environment...

:: Path to vswhere.exe
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%vswhere%" (
    echo Error: vswhere.exe not found at "%vswhere%"
    exit /b 1
)

:: Find the latest Visual Studio installation
set "vs_path="
for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -property installationPath`) do (
    set "vs_path=%%i"
)

if "%vs_path%"=="" (
    echo Error: Could not find Visual Studio installation.
    exit /b 1
)

echo Found Visual Studio at: %vs_path%

:: Find vcvarsall.bat
set "vcvarsall=%vs_path%\VC\Auxiliary\Build\vcvarsall.bat"

if not exist "%vcvarsall%" (
    echo Error: vcvarsall.bat not found at "%vcvarsall%"
    exit /b 1
)

:: Call vcvarsall.bat
:: Use "call" so it affects the current environment
echo Calling: "%vcvarsall%" x64
call "%vcvarsall%" x64

if %ERRORLEVEL% neq 0 (
    echo Error: Failed to initialize MSVC environment.
    exit /b %ERRORLEVEL%
)

echo MSVC environment initialized successfully.
exit /b 0
