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

# Shim entry points, kept in sync with the hand-curated user header
# filc/include/filc_llvm_jit.h by hand. Each entry is (table field,
# shim implementation). Signatures live in API_STRUCT_TEMPLATE below;
# the user header carries the matching declarations for Fil-C programs.
JIT_OWN = [
    ("Create", "shim_Create"),
    ("SetOption", "shim_SetOption"),
    ("DestroySession", "shim_DestroySession"),
    ("SetMemoryCallbacks", "shim_SetMemoryCallbacks"),
    ("CompileModule", "shim_CompileModule"),
    ("DestroyModule", "shim_DestroyModule"),
    ("AddGlobalMapping", "shim_AddGlobalMapping"),
    ("GetFunctionAddress", "shim_GetFunctionAddress"),
    ("GetGlobalValueAddress", "shim_GetGlobalValueAddress"),
    ("GetPointerToGlobal", "shim_GetPointerToGlobal"),
    ("GetLastError", "shim_GetLastError"),
]

JIT_INTERNAL = [
    ("ContextDispose", "LLVMContextDispose"),
    ("DisposeModule", "LLVMDisposeModule"),
    ("DisposeBuilder", "LLVMDisposeBuilder"),
]

DENY = set("""
LLVMShutdown
LLVMCreateMessage LLVMDisposeMessage
LLVMGetGlobalContext
LLVMContextSetDiagnosticHandler LLVMContextGetDiagnosticHandler LLVMContextGetDiagnosticContext
LLVMContextSetYieldCallback
LLVMGetDiagInfoDescription LLVMGetDiagInfoSeverity
LLVMContextDispose LLVMDisposeModule LLVMDisposeModuleFlagsMetadata LLVMDisposeBuilder
LLVMDisposeMemoryBuffer LLVMDisposeModuleProvider LLVMDisposePassManager LLVMDisposeOperandBundle
LLVMDisposeValueMetadataEntries
LLVMCreateMemoryBufferWithContentsOfFile LLVMCreateMemoryBufferWithMemoryRange
LLVMCreateMemoryBufferWithMemoryRangeCopy LLVMCreateMemoryBufferWithSTDIN
LLVMGetBufferStart LLVMGetBufferSize
LLVMPrintModuleToFile
LLVMDeleteFunction LLVMDeleteGlobal LLVMDeleteBasicBlock LLVMDeleteInstruction
LLVMInstructionEraseFromParent LLVMInstructionRemoveFromParent
LLVMRemoveBasicBlockFromParent LLVMRemoveGlobalIFunc LLVMEraseGlobalIFunc
LLVMGlobalClearMetadata LLVMGlobalEraseMetadata
LLVMCreatePassManager LLVMCreateFunctionPassManager LLVMCreateFunctionPassManagerForModule
LLVMRunPassManager LLVMRunFunctionPassManager LLVMInitializeFunctionPassManager
LLVMFinalizeFunctionPassManager
LLVMStartMultithreaded LLVMStopMultithreaded
LLVMCreateModuleProviderForExistingModule
LLVMPrintModuleToString LLVMPrintValueToString LLVMPrintTypeToString LLVMPrintDbgRecordToString
LLVMGetAsString
LLVMCopyModuleFlagsMetadata
LLVMCreateOperandBundle LLVMDisposeOperandBundle LLVMGetOperandBundleTag
LLVMGetOperandBundleArgAtIndex LLVMGetOperandBundleAtIndex LLVMGetNumOperandBundles
LLVMGetNumOperandBundleArgs
LLVMBuildCallWithOperandBundles LLVMBuildInvokeWithOperandBundles
LLVMBuildNUWNeg LLVMConstNUWNeg LLVMGetElementAsConstant
LLVMModuleCreateWithName LLVMCreateBuilder
LLVMAppendBasicBlock LLVMInsertBasicBlock
LLVMGetMDKindID
LLVMMDString LLVMMDNode
LLVMConstString LLVMConstStruct LLVMStructType
LLVMInt1Type LLVMInt8Type LLVMInt16Type LLVMInt32Type LLVMInt64Type
LLVMInt128Type LLVMIntType
LLVMHalfType LLVMBFloatType LLVMFloatType LLVMDoubleType
LLVMX86FP80Type LLVMFP128Type LLVMPPCFP128Type LLVMX86AMXType
LLVMVoidType LLVMLabelType
""".split())

# Substring assertions applied to the final list.  Each entry is
# (pattern, {allowed exceptions}).
BAD_PATTERNS = [
    ("Dispose", set()),
    ("Delete", set()),
    ("Erase", set()),
    ("RemoveFromParent", set()),
    ("Shutdown", set()),
    ("GlobalContext", set()),
    ("Diagnostic", set()),
    ("DiagInfo", set()),
    ("Yield", set()),
    ("PassManager", set()),
    ("ModuleProvider", set()),
    ("OperandBundle", set()),
    ("Multithreaded", {"LLVMIsMultithreaded"}),
    ("PrintModuleToString", set()),
    ("PrintValueToString", set()),
    ("PrintTypeToString", set()),
    ("PrintDbgRecord", set()),
    ("GetAsString", set()),
    ("CopyModuleFlags", set()),
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

    # Filter the denylist.
    found = {name for name, _ in functions}
    for banned in sorted(DENY):
        if banned not in found:
            print("warning: denylisted %s not present in Core.h" % banned,
                  file=sys.stderr)
    kept = [(n, s) for (n, s) in functions if n not in DENY]

    # Drop the LLVMIsA* declarations generated by the macro in Core.h; we
    # re-emit them from the subclass list so the set is explicit.
    kept = [(n, s) for (n, s) in kept
            if not (n.startswith("LLVMIsA") and n[8:] in seen)]
    kept = [(n, s) for (n, s) in kept if not n.startswith("LLVMIsA")]
    kept = kept + [("LLVMIsA%s" % n,
                    "LLVMValueRef LLVMIsA%s(LLVMValueRef Val);" % n)
                   for n in sorted(seen)]

    for pattern, allowed in BAD_PATTERNS:
        for name, _ in kept:
            if pattern in name and name not in allowed:
                raise SystemExit(
                    "error: excluded pattern %r matched %s" % (pattern, name))

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
        for proto_name, proto in kept:
            m = re.match(r"^(.+?[\s\*])(\w+)\s*\((.*)\)\s*;$", proto,
                         re.DOTALL)
            assert m and m.group(2) == proto_name, \
                "cannot make field from: %s" % proto
            llvm_fields += "    %s(*%s)(%s);\n" % (m.group(1).strip(),
                                                   proto_name, m.group(3).strip())
        api_out.write(API_STRUCT_TEMPLATE.replace(
            "@LLVM_FIELDS@", llvm_fields))
    print("wrote %s" % OUT_API)

    with open(OUT_INC, "w") as inc:
        inc.write("// Generated by gen_filc_llvm_jit_header.py; do not edit.\n")
        inc.write("    .version = FILC_LLVM_JIT_API_VERSION,\n")
        for field, impl in JIT_OWN + JIT_INTERNAL:
            inc.write("    .%s = %s,\n" % (field, impl))
        for name, _ in kept:
            inc.write("    .%s = %s,\n" % (name, name))
    print("wrote %s" % OUT_INC)


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
// filc/llvmjit/filc_llvm_jit_shim.cpp include it.  It carries the one
// definition of struct filc_llvm_jit_api plus the memory-callback struct the
// runtime installs with SetMemoryCallbacks.  The user header
// filc/include/filc_llvm_jit.h is hand-curated and carries only opaque refs
// and function declarations for Fil-C programs.

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

typedef struct FilLLVMOpaqueJIT *FilLLVMJITRef;
typedef struct FilLLVMOpaqueJITModule *FilLLVMJITModuleRef;

// User-provided lazy resolver.  Called as resolve(name, context) and must
// return the address the symbol resolves to, or NULL if unknown.
typedef void *(*FilLLVMJITResolveFunction)(const char *name, void *context);

// Shim-owned handles.  These name the same pointers the user sees through
// the opaque FilLLVMJITRef/FilLLVMJITModuleRef types, but the shim spells
// them with its own names so the manually managed session/module records
// are never confused with the user-facing opaque refs.
typedef struct ShimJitSession *ShimJit;
typedef struct ShimJitModuleRec *ShimJitModule;
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
#define FILC_LLVM_JIT_GC_SECTION_PREFIX "__filc_gc_obj"
#define FILC_LLVM_JIT_AUX_SECTION_PREFIX "__filc_jit_aux"

// Runtime-provided memory callbacks, installed with SetMemoryCallbacks
// before the first CompileModule.  The shim routes every LinkGraph section
// through these: LLVM never allocates executable memory on its own and the
// shim never touches libpas types (no pas_range, no object or alignment
// header shapes).  Every address is an opaque pointer and every size is a
// byte count; the runtime appends executable and object allocations to the
// owning jit_module so the module keeps them alive.
struct filc_llvm_jit_memory_callbacks {
    void *context;
    // Allocate the jit_module for one LinkGraph, reserving room for
    // combined_aux_size bytes of aux plus other_sections_size bytes of
    // default (non-object, non-executable) section data.  Sizes are upper
    // bounds covering per-section alignment slack.  Returns the opaque
    // jit_module pointer, or NULL on failure.
    void *(*allocate_jit_module)(void *context,
                                 unsigned long long combined_aux_size,
                                 unsigned long long other_sections_size);
    // Allocate executable memory for one executable section.  The runtime
    // appends the allocation to the module's executable array.  Returns the
    // base address, or NULL on failure.
    void *(*allocate_executable)(void *context, void *jit_module,
                                 unsigned long long size,
                                 unsigned long long alignment,
                                 const char *section_name);
    // Allocate one GC-managed object backing a GC section.  The returned
    // pointer names the payload area the section bytes are copied into; the
    // runtime accounts for its own alignment header outside the reported
    // size.  The runtime appends the object to the module's object array.
    // Returns the payload address, or NULL on failure.
    void *(*allocate_object)(void *context, void *jit_module,
                             const char *section_name,
                             unsigned long long size,
                             unsigned long long alignment);
    // Allocate room for one default section (aux sections land in the
    // combined-aux part of the module tail, the rest in the other part).
    // Returns the base address, or NULL on failure.
    void *(*allocate_module_data)(void *context, void *jit_module,
                                  const char *section_name,
                                  unsigned long long size,
                                  unsigned long long alignment);
    // Runs at link finalize after a GC section's bytes are in place: marks
    // the payload aligned (sets the alignment bits) and points the alignment
    // header's extra pointer at the jit_module, so any reference to the
    // object keeps the whole module alive.
    void (*finalize_object)(void *context, void *jit_module, void *object,
                            unsigned long long size);
    // Resolve an extern symbol name against one compiled module. The shim
    // calls this from JITed code (which passes its own jit_module, taken
    // from the filc_jit_module global) with the NUL-terminated name. The
    // runtime consults that module's resolver snapshot, records the result
    // as a module root, and returns the address, or NULL if unknown.
    void *(*resolve_symbol)(void *context, void *jit_module,
                            const char *name);
    // Resolve a zexact-encoded pointer against one compiled module. The
    // runtime consults that module's zexact table snapshot, records the
    // result as a module root, and returns the address, or NULL if unknown.
    void *(*resolve_raw_pointer)(void *context, void *jit_module,
                                 unsigned long long encoded);
};

struct filc_llvm_jit_api {
    unsigned long long version;
#define FILC_LLVM_JIT_API_VERSION 7ULL
    // JIT entry points (lazy auto-init: the first Create boots LLVM targets,
    // so no user-visible init entry exists).
    ShimJit (*Create)(void);
    int (*SetOption)(ShimJit jit, const char *key, const char *value);
    void (*DestroySession)(ShimJit jit);
    int (*SetMemoryCallbacks)(ShimJit jit,
                              const struct filc_llvm_jit_memory_callbacks *callbacks);
    ShimJitModule (*CompileModule)(ShimJit jit, LLVMModuleRef mod,
                                   const char *name, void *jit_module);
    void (*DestroyModule)(ShimJitModule mod);
    int (*AddGlobalMapping)(ShimJit jit, const char *name, void *address);
    void *(*GetFunctionAddress)(ShimJitModule mod, const char *name);
    void *(*GetGlobalValueAddress)(ShimJitModule mod, const char *name);
    void *(*GetPointerToGlobal)(ShimJitModule mod, const char *name);
    const char *(*GetLastError)(void);
    // Internal: disposal primitives for runtime object destructors.
    void (*ContextDispose)(LLVMContextRef C);
    void (*DisposeModule)(LLVMModuleRef M);
    void (*DisposeBuilder)(LLVMBuilderRef B);
    // Curated LLVM Core C API subset (same declarations as in
    // filc_llvm_jit.h, as function pointers for the
    // libpizlo -> libfilcllvmjit.so ABI).
@LLVM_FIELDS@
};


// The single exported entry point of libfilcllvmjit.so.
const struct filc_llvm_jit_api *FilLLVMJIT_GetApi(void);

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
