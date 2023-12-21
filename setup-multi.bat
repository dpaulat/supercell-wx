call tools\setup-common.bat
set build_dir=build
mkdir %build_dir%
cmake -B %build_dir% -S . -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=external/cmake-conan/conan_provider.cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.6.1/msvc2019_64
pause
