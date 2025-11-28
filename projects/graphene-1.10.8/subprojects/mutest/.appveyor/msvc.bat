@echo on

set MESON_VERSION="0.50.1"
set MUTEST_OUTPUT=tap

:: Download Meson and Ninja, create install directory
mkdir _build
mkdir mutest-static-%MSVC_PLATFORM%
cd _build
curl -LsSO https://github.com/mesonbuild/meson/releases/download/%MESON_VERSION%/meson-%MESON_VERSION%.tar.gz
7z x meson-%MESON_VERSION%.tar.gz
move dist\meson-%MESON_VERSION%.tar .
7z x meson-%MESON_VERSION%.tar
rmdir dist
del meson-%MESON_VERSION%.tar meson-%MESON_VERSION%.tar.gz
curl -LsSO https://github.com/ninja-build/ninja/releases/download/v1.7.2/ninja-win.zip
7z x ninja-win.zip
del ninja-win.zip
cd ..

:: Build and install
cd _build
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %MSVC_PLATFORM%
@echo on
C:\Python36\python.exe meson-%MESON_VERSION%\meson.py .. . -Dstatic=true --backend=ninja --prefix=%APPVEYOR_BUILD_FOLDER%\mutest-static-%MSVC_PLATFORM% || goto :error
ninja || goto :error
ninja test || goto :error
ninja install || goto :error
cd ..

:: Copy license into install directory and create .zip file
copy LICENSE.txt mutest-static-%MSVC_PLATFORM% || goto :error
dir mutest-static-%MSVC_PLATFORM% /s /b || goto :error
7z a -tzip mutest-static-%MSVC_PLATFORM%.zip mutest-static-%MSVC_PLATFORM% || goto :error

goto :EOF

:error
exit /b 1
