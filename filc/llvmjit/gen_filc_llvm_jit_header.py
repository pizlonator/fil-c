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

"""Generate filc/include/filc_llvm_jit.h from the in-tree LLVM C API headers.

The generated header exposes a curated subset of LLVM's Core C API (IR
builder plus most of Core) along with the FilLLVMJIT* JIT-compile API and
the FilLLVMJITApi function-pointer struct served by libfilcllvmjit.so.

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
OUT_H = os.path.join(REPO, "filc", "include", "filc_llvm_jit.h")
OUT_API = os.path.join(REPO, "filc", "llvmjit", "filc_llvm_jit_api.h")
OUT_INC = os.path.join(REPO, "filc", "llvmjit", "shim_api_init.inc")

JIT_OWN = [
    ("GetHeapMemory", "shim_GetHeapMemory"),
    ("Create", "shim_Create"),
    ("SetOption", "shim_SetOption"),
    ("SetResolveCallback", "shim_SetResolveCallback"),
    ("SetZExactPointerTable", "shim_SetZExactPointerTable"),
    ("AddModule", "shim_AddModule"),
    ("AddGlobalMapping", "shim_AddGlobalMapping"),
    ("GetFunctionAddress", "shim_GetFunctionAddress"),
    ("GetGlobalValueAddress", "shim_GetGlobalValueAddress"),
    ("GetPointerToGlobal", "shim_GetPointerToGlobal"),
    ("GetLastError", "shim_GetLastError"),
]

# Test-only executor entries.  These are appended AFTER the curated LLVM
# fields at the end of struct FilLLVMJITApi (which lives in the internal
# filc_llvm_jit_api.h, never in the user header) so every pre-existing field
# keeps its offset (the runtime links against an older copy of this table
# and reads fields positionally through its own compiled-in copy; middle
# insertion would silently misread the table).  Appending InvokeIntFunction
# changed sizeof(struct FilLLVMJITApi), so an old runtime reading a new
# table (or vice versa) would read out of bounds: hence API version 2.
# Removing the Initialize entry changed every subsequent field's offset, so
# the version went 2 -> 3 and the runtime requires exactly v3 (no
# back-compat with v1/v2 tables, whose layouts differ).  Removing the
# global-context creators (LLVMModuleCreateWithName, LLVMCreateBuilder,
# LLVMAppendBasicBlock, LLVMInsertBasicBlock, the non-InContext type
# constructors, LLVMMDString, LLVMMDNode, LLVMConstString, LLVMConstStruct,
# LLVMStructType, LLVMGetMDKindID) changed every subsequent field's offset,
# so the version went 3 -> 4 and the runtime requires exactly v4 (no
# back-compat with v1/v2/v3 tables, whose layouts differ).  Version history:
# 1 = pre-InvokeIntFunction table; 2 = with InvokeIntFunction tail; 3 =
# without Initialize (lazy auto-init: no user-visible init entry); 4 =
# without global-context creators (explicit InContext only).
# See filc_llvm_jit_ensure_api_slow and
# filc_native_FilLLVMJIT_InvokeIntFunction in
# libpas/src/libpas/filc_llvm_jit.c.
JIT_TAIL = [
    ("InvokeIntFunction", "shim_InvokeIntFunction"),
]

# Prototypes backing JIT_TAIL (kept in sync with the declarations emitted
# into the JIT_API template below; the generator parses these back into
# struct fields, so edit both together).
TAIL_PROTOS = """
int FilLLVMJIT_InvokeIntFunction(FilLLVMJITRef jit, const char *name, int *out);
"""

JIT_INTERNAL = [
    ("ContextDispose", "LLVMContextDispose"),
    ("DisposeModule", "LLVMDisposeModule"),
    ("DisposeBuilder", "LLVMDisposeBuilder"),
    ("ResolveByName", "FilLLVMJIT_shim_resolveByName"),
    ("ResolveRawPointer", "FilLLVMJIT_shim_resolveRaw"),
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

    with open(OUT_H, "w") as out:
        out.write(HEADER_PREAMBLE)
        out.write("\n/* Opaque LLVM types and basic C API types.  This header is\n")
        out.write("   self-contained: it never includes <llvm-c/...> and never tests\n")
        out.write("   for LLVM_C_*_H.  Include ONLY this header, never the real\n")
        out.write("   llvm-c headers alongside it. */\n")
        for td in typedefs:
            out.write(td + "\n")
        out.write("\n/* Enumerations from llvm-c/Core.h (minus diagnostics). */\n")
        for e in enums:
            out.write(e + "\n")
        for t in plain_typedefs:
            out.write(t + "\n")
        out.write("\n/* Curated LLVM Core C API subset.  No destruction, disposal,\n")
        out.write("   shutdown, global-context, diagnostic, or yield operations. */\n")
        for _, proto in kept:
            out.write(proto + "\n")
        out.write(JIT_API_USER)
    print("wrote %s" % OUT_H)

    with open(OUT_API, "w") as api_out:
        api_out.write(API_PREAMBLE)
        api_out.write("\n/* Opaque LLVM types and basic C API types.\n")
        api_out.write("   If the real llvm-c headers were already included (as they are\n")
        api_out.write("   in the shim, which needs the real LLVM C++ API), defer to\n")
        api_out.write("   them.  The Yolo-compiled runtime includes only this header,\n")
        api_out.write("   so it gets these definitions from here. */\n")
        api_out.write("#ifndef LLVM_C_TYPES_H\n")
        for td in typedefs:
            api_out.write(td + "\n")
        api_out.write("#endif /* LLVM_C_TYPES_H */\n")
        api_out.write("\n/* Enumerations from llvm-c/Core.h (minus diagnostics).\n")
        api_out.write("   Guarded so the shim, which includes the real llvm-c\n")
        api_out.write("   headers first, defers to them. */\n")
        api_out.write("#ifndef LLVM_C_CORE_H\n")
        for e in enums:
            api_out.write(e + "\n")
        for t in plain_typedefs:
            api_out.write(t + "\n")
        api_out.write("#endif /* LLVM_C_CORE_H */\n")
        llvm_fields = ""
        for proto_name, proto in kept:
            m = re.match(r"^(.+?[\s\*])(\w+)\s*\((.*)\)\s*;$", proto,
                         re.DOTALL)
            assert m and m.group(2) == proto_name, \
                "cannot make field from: %s" % proto
            llvm_fields += "    %s(*%s)(%s);\n" % (m.group(1).strip(),
                                                   proto_name, m.group(3).strip())
        tail_fields = ""
        for field, impl in JIT_TAIL:
            _ = impl
            found = False
            for line in TAIL_PROTOS.splitlines():
                m = re.match(r"^\s*(.+?[\s\*])(\w+)\s*\((.*)\)\s*;\s*$",
                             line)
                if m and m.group(2) == "FilLLVMJIT_" + field:
                    tail_fields += "    %s(*%s)(%s);\n" % (
                        m.group(1).strip(), field, m.group(3).strip())
                    found = True
            assert found, "JIT_TAIL entry without TAIL_PROTOS prototype: %s" % field
        api_out.write(API_STRUCT_TEMPLATE.replace(
            "@LLVM_FIELDS@", llvm_fields).replace(
            "@TAIL_FIELDS@", tail_fields))
    print("wrote %s" % OUT_API)

    with open(OUT_INC, "w") as inc:
        inc.write("/* Generated by gen_filc_llvm_jit_header.py; do not edit. */\n")
        inc.write("    .version = FILC_LLVM_JIT_API_VERSION,\n")
        for field, impl in JIT_OWN + JIT_INTERNAL:
            inc.write("    .%s = %s,\n" % (field, impl))
        for name, _ in kept:
            inc.write("    .%s = %s,\n" % (name, name))
        for field, impl in JIT_TAIL:
            inc.write("    .%s = %s,\n" % (field, impl))
    print("wrote %s" % OUT_INC)


HEADER_PREAMBLE = """/*
   Copyright (c) 2026 Filip Pizlo. All Rights Reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
   OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Fil-C LLVM JIT API.

   This header exposes a curated subset of the LLVM C API (IR building plus
   most of Core) together with the FilLLVMJIT* JIT-compile API.  All LLVM*Ref
   pointer types are opaque.  This header is a standalone substitute for the
   llvm-c headers: include this header INSTEAD of <llvm-c/Core.h> et al,
   never both.  It is self-contained (only <stddef.h> and <stdint.h>) and
   deliberately defines its contents unconditionally: it never includes
   <llvm-c/...> and never tests for LLVM_C_*_H.  Yolo-compiled code in
   particular must never include this header; the runtime and the shim share
   the internal filc_llvm_jit_api.h instead (generated alongside this file,
   never installed).

   Deliberately NOT exposed here:
    - destructive operations bottoming out in eraseFromParent,
      removeFromParent, or delete,
    - LLVMDispose* of any kind,
    - LLVMShutdown and LLVMGetGlobalContext,
    - global-context creators without an LLVMContextRef parameter (use the
      InContext sibling: LLVMModuleCreateWithNameInContext,
      LLVMCreateBuilderInContext, LLVMAppendBasicBlockInContext, and the
      other ...InContext forms),
    - diagnostic-handler and yield-callback operations,
    - APIs returning owned strings that would need LLVMDisposeMessage.

   Object lifetimes are managed by the Fil-C runtime (filc_llvm_jit.c); user
   code never disposes of LLVM objects directly.
 */

#ifndef FILC_LLVM_JIT_H
#define FILC_LLVM_JIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
} /* tell emacs what is up */
#endif
"""

JIT_API_USER = """
/* FilLLVMJIT* JIT-compile API.

   Typical flow (no explicit initialization: the first call that needs the
   shim lazily dlopens it and boots LLVM's native target plus the JIT heap
   automatically):
     FilLLVMJITRef jit = FilLLVMJIT_Create(NULL);      // NULL selects host
     FilLLVMJIT_SetOption(jit, "opt-level", "2");      // optional tuning
     FilLLVMJIT_SetResolveCallback(jit, my_resolve, ctx);
     FilLLVMJIT_SetZExactPointerTable(jit, table);
     FilLLVMJITModuleRef modref = 0;
     FilLLVMJIT_AddModule(jit, module, "mymod", &modref);
     void *fn = FilLLVMJIT_GetFunctionAddress(jit, "myfunc");

   Compile options (set with FilLLVMJIT_SetOption prior to compilation):
     "opt-level"                  "0".."3"   (default "2")
     "verify"                     "0"/"1"    (default "1")
     "opt-level"                  "0".."3"   (default "2")
     "verify"                     "0"/"1"    (default "1")
     "filc-optimize"              "0"/"1"    (default "1")
     "filc-inline"                "0"/"1"    (default "1")
     "filc-inline-sroa"           "0"/"1"    (default "1")
     "filc-inline-early-cse"      "0"/"1"    (default "1")
     "filc-inline-jump-threading" "0"/"1"    (default "1")
     "filc-inline-cvp"            "0"/"1"    (default "1")
     "filc-inline-simplify-cfg"   "0"/"1"    (default "1")
     "filc-inline-inst-combine-early" "0"/"1" (default "1")
     "filc-inline-inst-combine-late"  "0"/"1" (default "1")
     "filc-inline-gvn"            "0"/"1"    (default "1")
     "filc-inline-sccp"           "0"/"1"    (default "1")
     "filc-inline-adce"           "0"/"1"    (default "1")
     "filc-inline-dse"            "0"/"1"    (default "1")
     "filc-dse"                   "0"/"1"    (default "0")

   KillUB and the FilPizlonator pass are NOT optional: they always run,
   regardless of options.  There is no key that disables them; legacy
   spellings ("filc-killub", "killub", "filc-pizlonator", "pizlonator",
   "enable-pizlonator") are rejected with an error.

   Returns: FilLLVMJIT_*_functions returning int yield 0 on success and
   nonzero on failure; see FilLLVMJIT_GetLastError for details.
 */

typedef struct FilLLVMOpaqueJIT *FilLLVMJITRef;
typedef struct FilLLVMOpaqueJITModule *FilLLVMJITModuleRef;

/* User-provided lazy resolver.  Called as resolve(name, context) and must
   return the address the symbol resolves to, or NULL if unknown. */
typedef void *(*FilLLVMJITResolveFunction)(const char *name, void *context);

/* Returns the RWX mapping the shim booted for JIT memory on first use
   (base and size in bytes).  Lets the Fil-C runtime feed the range to
   jit_heap_add_fresh_memory itself if it prefers.  Calling this also
   triggers the same lazy one-time initialization as Create. */
int FilLLVMJIT_GetHeapMemory(void **base, unsigned long long *size);

/* Create a JIT session.  triple may be NULL to select the host triple. */
FilLLVMJITRef FilLLVMJIT_Create(const char *triple);

/* Set a key/value compile option (see list above).  Unknown keys fail. */
int FilLLVMJIT_SetOption(FilLLVMJITRef jit, const char *key, const char *value);

/* Install the lazy-resolution callback invoked by the host-side
   filc_llvm_jit_resolve whenever JITed code references an extern symbol.
   Stored per session; each module compiled by this session resolves through
   this callback (never another session's), via a session-carrying wrapper
   the shim authors at AddModule time. */
void FilLLVMJIT_SetResolveCallback(FilLLVMJITRef jit,
                                   FilLLVMJITResolveFunction callback,
                                   void *context);

/* Install the zexact_ptrtable used by zexternptr / raw-pointer resolution.
   The table pointer is a (struct zexact_ptrtable *) from the Fil-C runtime,
   kept opaque here.  Stored per session; each module compiled by this
   session decodes through this table (never another session's). */
int FilLLVMJIT_SetZExactPointerTable(FilLLVMJITRef jit, void *table);

/* Run the Fil-C pass pipeline over a copy of mod, add it to the JIT under
   the given name, and return an opaque module handle.  The caller's module
   is left unmodified.  Extern declarations matching AddGlobalMapping entries
   are bound to their mapped addresses during compilation: raw-ABI helpers
   (filc_llvm_jit_resolve, filc_llvm_jit_resolve_raw_pointer, zexternptr)
   are rebound to inttoptr constants directly, while other function names
   keep their declarations so FilPizlonator authors lazy-extern workers for
   them, whose bodies are then stubbed to return the mapped address wrapped
   as a flight {addr,addr} (a raw ptr constant would be a type mismatch
   against the flight-pointer ABI the call sites lower to). */
int FilLLVMJIT_AddModule(FilLLVMJITRef jit, LLVMModuleRef mod, const char *name,
                         FilLLVMJITModuleRef *out);

/* Bind an extern symbol name to a host address for subsequently compiled
   modules (AddGlobalMapping-style hook).  For function names the address
   must be the function's payload (upper); JITed callers reach it through
   the stubbed lazy-extern worker as the flight {addr,addr}.  In particular,
   IR that declares and calls `void *zexternptr(unsigned long long)` gets
   lazy resolution through the installed zexact_ptrtable via the
   filc_llvm_jit_resolve_raw_pointer path. */
int FilLLVMJIT_AddGlobalMapping(FilLLVMJITRef jit, const char *name, void *address);

/* Address lookup.  All three return NULL when the name is not found; names
   are unmangled IR names.  Because the Fil-C pass pipeline (FilPizlonator)
   renames functions to their pizlonated ABI form, each lookup transparently
   retries with a "pizlonated_" prefix when the exact name misses, and
   returns the JITed (pizlonated-ABI) address.  The returned code follows
   the pizlonated calling convention and is meant to be consumed through
   the Fil-C runtime, not called directly from ordinary C. */
void *FilLLVMJIT_GetFunctionAddress(FilLLVMJITRef jit, const char *name);
void *FilLLVMJIT_GetGlobalValueAddress(FilLLVMJITRef jit, const char *name);
void *FilLLVMJIT_GetPointerToGlobal(FilLLVMJITRef jit, const char *name);

/* Human-readable description of the most recent failure on this thread. */
const char *FilLLVMJIT_GetLastError(void);

/* Test-only executor: synchronously run a JITed no-argument function
   returning int and capture its result.

   Typical proof flow: AddGlobalMapping a genuine
   Fil-C function payload under "host_add", AddModule a caller module that
   declares `extern host_add` and defines `run` calling host_add(40, 2),
   then FilLLVMJIT_InvokeIntFunction(jit, "run", &out) executes the JITed
   `run` and stores 42 into *out.  The stubbed __filc_jit_get_host_add worker
   returns the mapped payload as the flight {addr,addr} -- exactly what
   FilPizlonator's own lazy worker would produce after resolving the same
   address -- so the in-JIT call path (pizlonated getter -> signature-checked
   thunk -> fast entrypoint) runs end to end.

   Scope and limits (this is bring-up tooling, not a calling convention):
    - only fast-CC `int name(void)` functions are supported;
    - the getter is invoked with the hosting Fil-C thread when visible (via
      filc_get_my_thread), else a private zeroed test thread, so only
      straight-line callees that touch no thread state beyond the
      stack-limit word are meaningful outside a Fil-C process;
    - a raised Fil-C exception in the callee yields an error, not a value;
    - panics inside JITed code abort the process.
   Reachable two ways: from Fil-C programs through the
   FilLLVMJIT_InvokeIntFunction native (same name, plus the session
   and an out pointer; returns the status), and from a host harness through
   the shim's function-pointer table.  Returns 0 on success (*out
   set), nonzero on failure; see FilLLVMJIT_GetLastError for details.
 */
int FilLLVMJIT_InvokeIntFunction(FilLLVMJITRef jit, const char *name, int *out);

/* Test-only resolution probes.

   These synchronous helpers resolve an extern name (or a raw encoded
   pointer) exactly as JITed code would through the lazy-resolution path,
   recording the resolved flight as a GC root in the given generated module
   (pass NULL to use the current session/module).  They exist for the
   root-stress tests; ordinary programs should use AddModule/GetFunctionAddress
   and real JITed callers instead.
 */
void *FilLLVMJIT_Resolve(void *module, const char *name);
void *FilLLVMJIT_ResolveRawPointer(void *module, void *table,
                                   unsigned long long encoded);

#if 0
{ /* tell emacs what is up */
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FILC_LLVM_JIT_H */
"""

API_PREAMBLE = """/*
   Copyright (c) 2026 Filip Pizlo. All Rights Reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
   OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Internal ABI between libpizlo (filc_llvm_jit.c) and libfilcllvmjit.so.

   This header is generated alongside filc/include/filc_llvm_jit.h by
   filc/llvmjit/gen_filc_llvm_jit_header.py.  It is NEVER installed into
   stdfil-include and must NEVER be included by Fil-C user code or by
   Yolo-compiled libpas files other than the runtime itself: only
   libpas/src/libpas/filc_llvm_jit.c and
   filc/llvmjit/filc_llvm_jit_shim.cpp include it.  It carries the one
   definition of struct FilLLVMJITApi; the user header carries only opaque
   refs and function declarations.  Keep the layout identical in both
   producers/consumers (v4 without global-context creators and without
   Initialize, with the Invoke tail) to
   avoid OOB reads.
 */

#ifndef FILC_LLVM_JIT_API_H
#define FILC_LLVM_JIT_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
} /* tell emacs what is up */
#endif

typedef struct FilLLVMOpaqueJIT *FilLLVMJITRef;
typedef struct FilLLVMOpaqueJITModule *FilLLVMJITModuleRef;

/* User-provided lazy resolver.  Called as resolve(name, context) and must
   return the address the symbol resolves to, or NULL if unknown. */
typedef void *(*FilLLVMJITResolveFunction)(const char *name, void *context);
"""

API_STRUCT_TEMPLATE = """
/* Internal ABI between libpizlo (filc_llvm_jit.c) and libfilcllvmjit.so.

   libfilcllvmjit.so exports exactly one symbol, FilLLVMJIT_GetApi, which
   returns a pointer to this struct.  The Fil-C runtime dlopens the library
   lazily on first use of any API declared in filc_llvm_jit.h and forwards
   calls through these pointers, so libpizlo itself never links LLVM.

   The "internal" entries at the end (LLVM disposal primitives plus the raw
   host-resolve helpers) exist so the runtime's LLVM_JIT object destructors
   and lazy-resolution trampolines can be implemented without linking LLVM.
   They are not for direct use by Fil-C programs.
 */
struct FilLLVMJITApi {
    unsigned long long version;
#define FILC_LLVM_JIT_API_VERSION 4ULL
    /* Version history: 1 = table without the InvokeIntFunction tail; 2 =
       with it; 3 = without the Initialize entry (lazy auto-init: the shim
       boots LLVM targets and the JIT heap on first Create/GetHeapMemory, and
       the runtime boots-feeds-probes on first dlopen via GetHeapMemory, so
       no user-visible init entry exists); 4 = without the global-context
       creators (only explicit ...InContext forms remain).  Removing the
       global-context creators shifted every later field, so v4 tables are
       layout-incompatible with v1/v2/v3: the
       runtime requires exactly v4. */
    /* FilLLVMJIT* entry points (no Initialize: lazy auto-init only). */
    int (*GetHeapMemory)(void **base, unsigned long long *size);
    FilLLVMJITRef (*Create)(const char *triple);
    int (*SetOption)(FilLLVMJITRef jit, const char *key, const char *value);
    void (*SetResolveCallback)(FilLLVMJITRef jit, FilLLVMJITResolveFunction callback,
                               void *context);
    int (*SetZExactPointerTable)(FilLLVMJITRef jit, void *table);
    int (*AddModule)(FilLLVMJITRef jit, LLVMModuleRef mod, const char *name,
                     FilLLVMJITModuleRef *out);
    int (*AddGlobalMapping)(FilLLVMJITRef jit, const char *name, void *address);
    void *(*GetFunctionAddress)(FilLLVMJITRef jit, const char *name);
    void *(*GetGlobalValueAddress)(FilLLVMJITRef jit, const char *name);
    void *(*GetPointerToGlobal)(FilLLVMJITRef jit, const char *name);
    const char *(*GetLastError)(void);
    /* Internal: disposal primitives for runtime object destructors. */
    void (*ContextDispose)(LLVMContextRef C);
    void (*DisposeModule)(LLVMModuleRef M);
    void (*DisposeBuilder)(LLVMBuilderRef B);
    /* Internal: raw host-resolve helpers behind the blessed mappings. */
    void *(*ResolveByName)(const char *name);
    void *(*ResolveRawPointer)(unsigned long long encoded);
    /* Curated LLVM Core C API subset (same declarations as in
       filc_llvm_jit.h, as function pointers for the
       libpizlo -> libfilcllvmjit.so ABI). */
@LLVM_FIELDS@
    /* Test-only executor entries appended after the LLVM block so every
       field above keeps its historical offset within v4 (see JIT_TAIL).
       InvokeIntFunction is the v2 tail, still present in v4; readers must
       still check the version before touching the tail. */
@TAIL_FIELDS@
};


/* The single exported entry point of libfilcllvmjit.so. */
const struct FilLLVMJITApi *FilLLVMJIT_GetApi(void);

#if 0
{ /* tell emacs what is up */
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FILC_LLVM_JIT_API_H */
"""

if __name__ == "__main__":
    main()
