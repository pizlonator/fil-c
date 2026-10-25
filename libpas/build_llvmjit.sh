#!/bin/sh

# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Builds libfilcllvmjit.so (the Fil-C LLVM JIT shim) into
# ../pizfix/libexec/.  Run from the libpas directory, e.g. via
# ./build_llvmjit.sh (also hooked from ../build_runtime.sh).
#
# The shim links the in-tree LLVM static libraries, including the ORC JIT
# stack.  Most LLVM archives ship prebuilt in ../build/lib; the JIT-only
# ones are built on demand with a targeted ninja invocation (additive only;
# it never rebuilds anything else).  Set FILC_LLVMJIT_SKIP_NINJA=1 to skip
# that step, or CXX to pick a different host compiler.

set -e
set -x

cd "$(dirname "$0")"

# Regenerate the public header, the internal runtime<->shim ABI header, and
# the shim's initializer table from the in-tree LLVM C API (keeps
# declarations and struct fields in sync).
python3 ../filc/llvmjit/gen_filc_llvm_jit_header.py

if [ "${FILC_LLVMJIT_SKIP_NINJA:-0}" != "1" ]; then
    ninja -C ../build \
        lib/libLLVMOrcJIT.a \
        lib/libLLVMJITLink.a \
        lib/libLLVMExecutionEngine.a \
        lib/libLLVMOrcShared.a \
        lib/libLLVMOrcTargetProcess.a \
        lib/libLLVMRuntimeDyld.a
fi

mkdir -p ../pizfix/libexec

CXX="${CXX:-/usr/bin/c++}"

SHIM_OBJ="$(mktemp /tmp/filc_llvm_jit_shim.XXXXXX.o)"

# NOTE: the in-tree LLVM build has RTTI and C++ exceptions disabled
# (LLVM_ENABLE_RTTI=OFF, LLVM_ENABLE_EH=OFF), so the shim must match with
# -fno-rtti -fno-exceptions; otherwise the TU emits references to LLVM
# typeinfo/vtable symbols that the static archives do not provide.
"$CXX" -g -O2 -fPIC -fvisibility=hidden -fno-rtti -fno-exceptions -std=c++20 \
    -I ../llvm/include -I ../build/include -I ../filc/include \
    -c ../filc/llvmjit/filc_llvm_jit_shim.cpp -o "$SHIM_OBJ"

# The shim is dlopened from inside Fil-C processes, whose loader only
# resolves libraries shipped in pizfix (plus process-global symbols from the
# already-loaded Fil-C userland, which provides versioned GLIBC symbols).
# A normal hosted link would add NEEDED entries for the host's
# libstdc++.so.6/libgcc_s/libc/libm, none of which exist in pizfix, so the
# dlopen would fail. Link with -nodefaultlibs and static libstdc++/libgcc
# instead: the .so then carries no shared NEEDEDs, and its remaining
# undefined (libc/pthread/dl/math) references bind to the hosting Fil-C
# process's globals at load time. The driver's crt files are still linked
# (only the default -l* set is suppressed), so C++ static initializers run.
STDCXX_A="$("$CXX" -print-file-name=libstdc++.a)"
LIBGCC_A="$("$CXX" -print-file-name=libgcc.a)"
LIBGCC_EH_A="$("$CXX" -print-file-name=libgcc_eh.a)"

"$CXX" -g -shared -nodefaultlibs -o ../pizfix/libexec/libfilcllvmjit.so "$SHIM_OBJ" \
    -Wl,--version-script=../filc/llvmjit/filc_llvm_jit_shim.version \
    -Wl,--exclude-libs,ALL \
    -Wl,--start-group ../build/lib/libLLVM*.a -Wl,--end-group \
    "$STDCXX_A" "$LIBGCC_EH_A" "$LIBGCC_A"

rm -f "$SHIM_OBJ"

# Split off LLVM's (huge) debug info into a separate file so the installed
# .so stays lean while remaining fully debuggable via the debug link.
# The shim object itself is still compiled with -g.
objcopy --only-keep-debug ../pizfix/libexec/libfilcllvmjit.so \
    ../pizfix/libexec/libfilcllvmjit.so.debug
strip -g ../pizfix/libexec/libfilcllvmjit.so
objcopy --add-gnu-debuglink=../pizfix/libexec/libfilcllvmjit.so.debug \
    ../pizfix/libexec/libfilcllvmjit.so

set +x

echo LLVM JIT shim installed.
