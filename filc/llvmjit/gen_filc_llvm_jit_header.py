#!/usr/bin/env python3
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

"""Generate filc/llvmjit/filc_llvm_jit_api.h (and shim_api_init.inc) from the
in-tree LLVM C API headers.

The generated internal header carries the one definition of struct
filc_llvm_jit_api, the runtime<->shim ABI table served by
libfilcllvmjit.so, plus the curated LLVM Core C API subset as function
pointers. It is NEVER installed and must NEVER be included by Fil-C user
code: only libpas/src/libpas/filc_llvm_jit.c and
filc/llvmjit/filc_llvm_jit_shim.cpp include it.

The user-visible header filc/include/filc_llvm_jit.h is HAND-CURATED and
tracked in git. This script never writes it; curate that file by editing it
directly. The JIT-entry declarations below are kept in sync with the user
header by hand.

Functions are deliberately excluded when they:
  - destroy IR (anything bottoming out in eraseFromParent,
    removeFromParent, or delete),
  - dispose of LLVM objects (LLVMDispose*),
  - shut down LLVM or fetch the global context,
  - implicitly use the global LLVM context (creators without an
    LLVMContextRef parameter: use the InContext sibling instead),
  - install/observe diagnostics or yield callbacks,
  - return owned strings that would require LLVMDisposeMessage to free,
  - belong to the legacy pass-manager or module-provider C APIs.

Usage: python3 filc/llvmjit/gen_filc_llvm_jit_header.py
Runs from the repository root.
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORE_H = os.path.join(REPO, "llvm", "include", "llvm-c", "Core.h")
TYPES_H = os.path.join(REPO, "llvm", "include", "llvm-c", "Types.h")
OUT_API = os.path.join(REPO, "filc", "llvmjit", "filc_llvm_jit_api.h")
OUT_INC = os.path.join(REPO, "filc", "llvmjit", "shim_api_init.inc")
OUT_RUNTIME_INC = os.path.join(REPO, "filc", "llvmjit", "runtime_api_init.inc")
OUT_MAPPING_INC = os.path.join(REPO, "filc", "llvmjit", "mapping_init.inc")

# Shim entry points, kept in sync with the hand-curated user header
# filc/include/filc_llvm_jit.h by hand. Each entry is (table field,
# shim implementation). Signatures live in API_STRUCT_TEMPLATE below;
# the user header carries the matching declarations for Fil-C programs.
JIT_OWN = [
    ("ShimJIT*", "ShimJITCreate", "(void)"),
    ("void", "ShimJITSetOption", "(ShimJIT* jit, const char* key, const char* value)"),
    ("void", "ShimJITSetGlobalMapping", "(ShimJIT* jit, const char* key, shim_ptr ptr)"),
    ("void", "ShimJITSetPointerMapping", "(ShimJIT* jit, shim_ptr ptr)"),
    ("void*", "ShimJITCompile", "(void*, void* jit_session, ShimJIT* jit, LLVMModuleRef mod)"),
    ("void", "ShimJITDestroy", "(ShimJIT* jit)"),
    ("void*", "ShimExportMappingLookup", "(ShimExportMapping* mapping, const char* name)"),
    ("void", "ShimExportMappingDestroy", "(ShimExportMapping* mapping)")
]

RUNTIME_COMPILETIME_API = [
    ("void*", "shim_allocate_executable_module", "(void* my_thread, void* jit_session)"),
    ("void*", "shim_executable_module_allocate_object", "(void* my_thread, void* executable_module, size_t size, size_t alignment)"),
    ("void*", "shim_executable_module_allocate_data", "(void* my_thread, void* executable_module, size_t size, size_t alignment)"),
    ("void*", "shim_executable_module_allocate_executable", "(void* my_thread, void* executable_module, size_t size)"),
    ("void*", "shim_executable_module_allocate_aux", "(void* my_thread, void* executable_module, size_t size)"),
    ("void", "shim_executable_module_finalize_objects", "(void* my_thread, void* executable_module)"),
    ("void", "shim_executable_module_set_export_mapping", "(void* my_thread, void* executable_module, ShimExportMapping* mapping)"),
    ("void", "pas_panic", "(const char* format, ...)"),
    ("void", "pas_log", "(const char* format, ...)")
]

RUNTIME_RUNTIME_API = [
    "filc_allocate",
    "filc_stack_overflow_failure",
    "filc_check_function_call_fail"
]

def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    # Drop full-line // comments (doxygen /// and plain //).
    lines = []
    for line in text.splitlines():
        if line.strip().startswith("//"):
            continue
        lines.append(line)
    return "\n".join(lines)


def logical_statements(path):
    """Yield top-level statements, skipping preprocessor directives."""
    with open(path) as f:
        raw = f.read()
    raw = strip_comments(raw)
    lines = raw.splitlines()
    # Drop preprocessor lines including backslash continuations.
    kept = []
    cont = False
    for line in lines:
        stripped = line.strip()
        if cont:
            cont = line.rstrip().endswith("\\")
            continue
        if stripped.startswith("#"):
            cont = line.rstrip().endswith("\\")
            continue
        kept.append(line)
    text = "\n".join(kept)
    # Drop marker lines that carry no semicolon of their own; they would
    # otherwise glue themselves to the adjacent declaration.
    cleaned = []
    for line in text.splitlines():
        s = line.strip()
        if s in ("LLVM_C_EXTERN_C_BEGIN", "LLVM_C_EXTERN_C_END",
                 "LLVM_C_STRICT_PROTOTYPES_BEGIN",
                 "LLVM_C_STRICT_PROTOTYPES_END"):
            continue
        if s.startswith("LLVM_FOR_EACH_VALUE_SUBCLASS("):
            continue
        if s.startswith("LLVM_DECLARE_VALUE_CAST("):
            continue
        cleaned.append(line)
    text = "\n".join(cleaned)
    # Split into statements at top-level semicolons.
    stmts = []
    depth_paren = 0
    depth_brace = 0
    buf = []
    for ch in text:
        buf.append(ch)
        if ch == "(":
            depth_paren += 1
        elif ch == ")":
            depth_paren -= 1
        elif ch == "{":
            depth_brace += 1
        elif ch == "}":
            depth_brace -= 1
        elif ch == ";" and depth_paren == 0 and depth_brace == 0:
            stmts.append("".join(buf))
            buf = []
    return stmts


def main():
    typedefs = []
    for stmt in logical_statements(TYPES_H):
        s = " ".join(stmt.split())
        if not s.startswith("typedef "):
            continue
        if "LLVM_C_EXTERN_C_BEGIN" in s or "LLVM_C_EXTERN_C_END" in s:
            continue
        typedefs.append(s)

    enums = []
    plain_typedefs = []
    functions = []
    subclass_names = []
    for stmt in logical_statements(CORE_H):
        s = " ".join(stmt.split())
        if "LLVM_C_EXTERN_C_BEGIN" in s or "LLVM_C_EXTERN_C_END" in s:
            continue
        if s.startswith("typedef enum"):
            m = re.search(r"}\s*(\w+)\s*;$", s)
            assert m, "cannot parse enum: %s" % s[:120]
            name = m.group(1)
            if name == "LLVMDiagnosticSeverity":
                continue
            enums.append(s)
            continue
        if s.startswith("typedef "):
            # Plain (non-enum, non-function) typedefs like
            # "typedef unsigned LLVMAttributeIndex;".
            if re.match(r"^typedef\s+[\w\s]+\s\w+\s*;$", s) and "(" not in s:
                plain_typedefs.append(s)
            continue
        m = re.match(r"^(.+?[\s\*])(\w+)\s*\(.*\)\s*;$", s, re.DOTALL)
        if not m:
            continue
        name = m.group(2)
        if name.startswith("LLVM"):
            functions.append((name, s))

    # Expand LLVM_FOR_EACH_VALUE_SUBCLASS invocations found in Core.h.
    with open(CORE_H) as f:
        core_text = strip_comments(f.read())
    for m in re.finditer(r"^\s*macro\((\w+)\)", core_text, re.MULTILINE):
        subclass_names.append(m.group(1))
    is_a_decls = []
    seen = set()
    for name in subclass_names:
        if name in seen:
            continue
        seen.add(name)
        is_a_decls.append(
            "LLVMValueRef LLVMIsA%s(LLVMValueRef Val);" % name)

    kept = [(n, s) for (n, s) in functions]

    # Drop the LLVMIsA* declarations generated by the macro in Core.h; we
    # re-emit them from the subclass list so the set is explicit.
    kept = [(n, s) for (n, s) in kept
            if not (n.startswith("LLVMIsA") and n[8:] in seen)]
    kept = [(n, s) for (n, s) in kept if not n.startswith("LLVMIsA")]
    kept = kept + [("LLVMIsA%s" % n,
                    "LLVMValueRef LLVMIsA%s(LLVMValueRef Val);" % n)
                   for n in sorted(seen)]

    kept.sort(key=lambda pair: pair[0])
    print("keeping %d functions, %d enums, %d typedefs" %
          (len(kept), len(enums), len(typedefs)))
    print("note: filc/include/filc_llvm_jit.h is hand-curated and is left alone")

    with open(OUT_API, "w") as api_out:
        api_out.write(API_PREAMBLE)
        api_out.write("\n// Opaque LLVM types and basic C API types.\n")
        api_out.write("// If the real llvm-c headers were already included (as they are\n")
        api_out.write("// in the shim, which needs the real LLVM C++ API), defer to\n")
        api_out.write("// them.  The Yolo-compiled runtime includes only this header,\n")
        api_out.write("// so it gets these definitions from here.\n")
        api_out.write("#ifndef LLVM_C_TYPES_H\n")
        for td in typedefs:
            api_out.write(td + "\n")
        api_out.write("typedef void (*LLVMDiagnosticHandler)(LLVMDiagnosticInfoRef, void *);\n")
        api_out.write("typedef void (*LLVMYieldCallback)(LLVMContextRef, void *);\n")
        api_out.write("typedef int LLVMDiagnosticSeverity;\n")
        api_out.write("#endif // LLVM_C_TYPES_H\n")
        api_out.write("\n// Enumerations from llvm-c/Core.h (minus diagnostics).\n")
        api_out.write("// Guarded so the shim, which includes the real llvm-c\n")
        api_out.write("// headers first, defers to them.\n")
        api_out.write("#ifndef LLVM_C_CORE_H\n")
        for e in enums:
            api_out.write(e + "\n")
        for t in plain_typedefs:
            api_out.write(t + "\n")
        api_out.write("#endif // LLVM_C_CORE_H\n")
        llvm_fields = ""
        for return_type, name, params in JIT_OWN:
            llvm_fields += "    %s(*%s)%s;\n" % (return_type, name, params)
        for proto_name, proto in kept:
            m = re.match(r"^(.+?[\s\*])(\w+)\s*\((.*)\)\s*;$", proto,
                         re.DOTALL)
            assert m and m.group(2) == proto_name, \
                "cannot make field from: %s" % proto
            llvm_fields += "    %s(*%s)(%s);\n" % (m.group(1).strip(),
                                                   proto_name, m.group(3).strip())
        runtime_api = ""
        for return_type, name, params in RUNTIME_COMPILETIME_API:
            runtime_api += "    %s(*%s)%s;\n" % (return_type, name, params)
        for name in RUNTIME_RUNTIME_API:
            runtime_api += "    void* %s;\n" % (name)
        api_out.write(API_STRUCT_TEMPLATE.replace(
            "@LLVM_FIELDS@", llvm_fields).replace(
            "@RUNTIME_API@", runtime_api))
    print("wrote %s" % OUT_API)

    with open(OUT_INC, "w") as inc:
        inc.write("// Generated by gen_filc_llvm_jit_header.py; do not edit.\n")
        inc.write("    .version = SHIM_API_VERSION,\n")
        for _, name, _ in JIT_OWN:
            inc.write("    .%s = %s,\n" % (name, name))
        for name, _ in kept:
            inc.write("    .%s = %s,\n" % (name, name))
    print("wrote %s" % OUT_INC)

    with open(OUT_RUNTIME_INC, "w") as inc:
        inc.write("// Generated by gen_filc_llvm_jit_header.py; do not edit.\n")
        inc.write("    .version = SHIM_API_VERSION,\n")
        for _, name, _ in RUNTIME_COMPILETIME_API:
            inc.write("    .%s = %s,\n" % (name, name))
        for name in RUNTIME_RUNTIME_API:
            inc.write("    .%s = %s,\n" % (name, name))
    print("wrote %s" % OUT_RUNTIME_INC)

    with open(OUT_MAPPING_INC, "w") as inc:
        inc.write("// Generated by gen_filc_llvm_jit_header.py; do not edit.\n")
        for name in RUNTIME_RUNTIME_API:
            inc.write("    RuntimeMapping[\"%s\"] = runtime->%s;\n" % (name, name))
    print("wrote %s" % OUT_MAPPING_INC)


API_PREAMBLE = """// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Internal ABI between libpizlo (filc_llvm_jit.c) and libfilcllvmjit.so.
//
// This header is generated by filc/llvmjit/gen_filc_llvm_jit_header.py.  It
// is NEVER installed into stdfil-include and must NEVER be included by
// Fil-C user code or by Yolo-compiled libpas files other than the runtime
// itself: only libpas/src/libpas/filc_llvm_jit.c and
// filc/llvmjit/filc_llvm_jit_shim.cpp include it.

#ifndef FILC_LLVM_JIT_API_H
#define FILC_LLVM_JIT_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
} // tell emacs what is up
#endif

typedef struct ShimJIT ShimJIT;
typedef struct ShimExportMapping ShimExportMapping;
"""

API_STRUCT_TEMPLATE = """
// Internal ABI between libpizlo (filc_llvm_jit.c) and libfilcllvmjit.so.
//
// libfilcllvmjit.so exports exactly one symbol, FilLLVMJIT_GetApi, which
// returns a pointer to this struct.  The Fil-C runtime dlopens the library
// lazily on first use of any API declared in filc_llvm_jit.h and forwards
// calls through these pointers, so libpizlo itself never links LLVM.
//
// The "internal" entries below the JIT entries (LLVM disposal primitives)
// exist so the runtime's LLVM_JIT object destructors can be implemented
// without linking LLVM.  They are not for direct use by Fil-C programs.

// Section names the shim memory manager routes on.  FilPizlonator in JIT
// mode emits every GC-managed object into its own section whose name starts
// with FILC_LLVM_JIT_GC_SECTION_PREFIX, and all JIT aux data into a section
// whose name starts with FILC_LLVM_JIT_AUX_SECTION_PREFIX; every other
// section is a default section (executable ones go to the JIT heap, the rest
// into the module tail).  The shim only ever compares section-name prefixes;
// it never interprets object, header, or range shapes.
#define FILC_LLVM_JIT_GC_SECTION_PREFIX "filc_gc_obj"
#define FILC_LLVM_JIT_AUX_SECTION_PREFIX "filc_jit_aux"

#define SHIM_API_VERSION 7

typedef struct shim_ptr shim_ptr;
struct shim_ptr {
    void* ptr;
    void* lower;
};

#undef LLVM_ATTRIBUTE_C_DEPRECATED
#define LLVM_ATTRIBUTE_C_DEPRECATED(thing, string) thing

// Runtime-provided memory callbacks, installed with SetMemoryCallbacks
// before the first CompileModule.  The shim routes every LinkGraph section
// through these: LLVM never allocates executable memory on its own and the
// shim never touches libpas types (no pas_range, no object or alignment
// header shapes).  Every address is an opaque pointer and every size is a
// byte count; the runtime appends executable and object allocations to the
// owning jit_module so the module keeps them alive.
typedef struct shim_runtime_api shim_runtime_api;
struct shim_runtime_api {
    unsigned version;
@RUNTIME_API@
};

typedef struct shim_api shim_api;
struct shim_api {
    unsigned version;
@LLVM_FIELDS@
};


// The single exported entry point of libfilcllvmjit.so.
const struct shim_api *FilLLVMJIT_GetApi(const shim_runtime_api* runtimeAPI,
                                         const char* options);

#if 0
{ // tell emacs what is up
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FILC_LLVM_JIT_API_H
"""

if __name__ == "__main__":
    main()
