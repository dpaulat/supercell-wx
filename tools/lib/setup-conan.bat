@set script_dir=%~dp0

:: Configure default Conan profile
conan profile detect -e

:: Install selected Conan profile
conan config install "%script_dir%\..\conan\profiles\%conan_profile%" -tf profiles

:: If conan_build_profile is not set, use the same as conan_profile
if "%conan_build_profile%" == "" (
    set conan_build_profile=%conan_profile%
)

:: Install Conan packages
conan install "%script_dir%\..\.." ^
    --remote conancenter ^
    --build missing ^
    --profile:build %conan_build_profile% ^
    --profile:host %conan_profile% ^
    --settings:all build_type=%build_type% ^
    --output-folder "%build_dir%\conan"
