@echo on
:: vcvarsall.bat sets various env vars like PATH, INCLUDE, LIB, LIBPATH for the
:: specified build architecture
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
@echo on

:: FIXME: make warnings fatal
pip3 install --upgrade --user meson~=0.64 || goto :error
meson setup -Dbackend_max_links=1 -Ddebug=false -Dmedia-gstreamer=disabled -Dvulkan=disabled _build || goto :error
ninja -C _build || goto :error

goto :EOF
:error
exit /b 1
