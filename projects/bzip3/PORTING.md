
## General problems

1. `Makefile.am:8: error: 'pkgconfig_DATA' is used but 'pkgconfigdir' is undefined` => please install pkgconfig.

## Windows

Cross-compiling Windows binaries is supported:

```console
# For x86_64 (64bit)
$ ./configure CC=x86_64-w64-mingw32-gcc --host x86_64-w64-mingw32 --enable-static-exe
$ make

# For i686 (32bit)
$ ./configure CC=i866-w64-mingw32-gcc --host i686-w64-mingw32 --enable-static-exe
$ make
```

Static builds are recommended to avoid the pthread dynamic linking issue. If a dynamic library is desired, consider defining `BZIP3_DLL_EXPORT` or `BZIP3_DLL_IMPORT`.

## M1 MacOS

Make sure that you run `./configure` with `--disable-arch-native`.

## Emscripten

Assuming that asm.js code is desired:

```
emconfigure ./configure --without-pthread --host none-none-none CC=emcc "CFLAGS=-O2 -DBZIP3_VISIBLE=\"__attribute__((used))\""
make src/bzip3-libbz3.o
emcc -O2 src/bzip3-libbz3.o -o libbz3.js -sWASM=0 --memory-init-file 0 -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH -s 'EXPORTED_RUNTIME_METHODS=["UTF8ToString"]'
```

asm.js code size: 118KB (v1.1.7), 34K gzipped.
wasm+js stub code size: 76KB (v1.1.7), 26K gzipped.
