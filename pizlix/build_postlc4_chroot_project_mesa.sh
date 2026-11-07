#!/bin/bash

set -e
set -x

cd /sources

# Fil-C: preflight guards (design doc 6.8/6.9).  The runtime must carry the
# JIT entry points, and the chroot clang must accept -enable-filc-jit --
# mesa's llvmpipe calls the JIT API through libpizlo.so.
strings /usr/lib/libpizlo.so | grep -q FilLLVMJIT_CompileModule
clang -enable-filc-jit -### >/dev/null 2>&1 || clang -enable-filc-jit --version >/dev/null

# Fil-C: fake llvm-config + stub LLVM headers.  The stub include dir carries
# llvm/Config/llvm-config.h (version defines) and llvm-c/Core.h (a forwarder
# to filc_llvm_jit.h); the fake llvm-config answers meson's queries without a
# real LLVM installation.  See pizlix/filc-llvm-config/ in the repo.
mkdir -p /usr/lib/filc-llvm-include/llvm/Config /usr/lib/filc-llvm-include/llvm-c
cp -v /sources/filc-llvm-config/include/llvm/Config/llvm-config.h \
      /usr/lib/filc-llvm-include/llvm/Config/
cp -v /sources/filc-llvm-config/include/llvm-c/Core.h \
      /usr/lib/filc-llvm-include/llvm-c/
cp -v /sources/filc-llvm-config/llvm-config /usr/bin/llvm-config
chmod +x /usr/bin/llvm-config
llvm-config --version

rm -rf pizlonated-mesa
tar -xf pizlonated-mesa.tar.gz
cd pizlonated-mesa
# The tarball is packaged from projects/mesa.projeny (URL: mesa-24.1.5.tar.xz +
# Fil-C patch): version-script symbol visibility for dlopen'd dri modules,
# bochs-drm (QEMU std VGA) driver entry point + install name, XGETBV inline asm
# removal, sparse-array tag-bit removal, EGL native handle types as real
# pointers, EGLDevice handle zptrtable encoding, glsl_type name_id decoding,
# nir_src tagged-pointer removal, queryobj buffer pointer handling, and the
# older asm-disabling (meson.build, blake3) + glibc once_flag fixes.
# Sprint 2 (llvmpipe-on-Fil-C-JIT): gallivm converted from MCJIT to the
# FilLLVMJIT_* session API (lp_bld_misc.cpp -> lp_bld_misc.c), the pass
# manager / disposal / target-data paths deleted, function-pointer
# constants converted to named mapped hooks, and u_cpu_detect AVX
# detection restored via __cpu_indicator_init + __builtin_cpu_supports.
mkdir -v build
cd build
meson setup ..                 \
      --prefix=/usr            \
      --buildtype=debugoptimized \
      -D platforms=wayland     \
      -D gallium-drivers=swrast \
      -D vulkan-drivers=       \
      -D valgrind=disabled     \
      -D libunwind=disabled    \
      -D glx=disabled          \
      -D cpp_rtti=false
ninja
ninja install
cd ../..
rm -rf pizlonated-mesa
