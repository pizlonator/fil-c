/* Records a doc/implementation divergence in FilLLVMJIT_SetOption:
   filc/include/filc_llvm_jit.h documents "verify", "filc-optimize", all of
   the "filc-inline*" keys, and "filc-dse" as compile options, but the shim
   (filc/llvmjit/filc_llvm_jit_shim.cpp) only ever reads "opt-level" out of
   its options map — every other documented key is a silent no-op. This test
   feeds the session every documented-but-unimplemented key and then compiles
   a valid module and calls it; it records the divergence without depending
   on it: if those options are implemented later, this module is valid under
   any of their settings, so the test keeps passing. The tripwire for
   verify-on-invalid-IR (what "verify" would actually guard) is
   jit_bogus_ver_*. SetOption must be called on the session before
   FilLLVMJIT_CompileModule. */

#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

static const char* documentedButUnimplemented[] = {
    "verify",
    "filc-optimize",
    "filc-inline",
    "filc-inline-sroa",
    "filc-inline-early-cse",
    "filc-inline-jump-threading",
    "filc-inline-cvp",
    "filc-inline-simplify-cfg",
    "filc-inline-inst-combine-early",
    "filc-inline-inst-combine-late",
    "filc-inline-gvn",
    "filc-inline-sccp",
    "filc-inline-adce",
    "filc-inline-dse",
    "filc-dse",
};

static __attribute__((noinline)) int (*buildFn(void))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef paramTypes[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef result = LLVMBuildAdd(
        builder, LLVMGetParam(function, 0),
        LLVMConstInt(LLVMInt32TypeInContext(context), 1, false), "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    unsigned k;
    for (k = 0; k < sizeof(documentedButUnimplemented) / sizeof(const char*); ++k)
        FilLLVMJIT_SetOption(jit, documentedButUnimplemented[k], "1");

    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int) = buildFn();
    int i;
    for (i = 0; i < 5; ++i) {
        int input = (int)(intptr_t)opaque((void*)(intptr_t)(40 + (size_t)i));
        int r = test(input);
        if (r != input + 1) {
            fprintf(stderr, "input %d returned %d\n", input, r);
            return 1;
        }
    }

    printf("setopt docdivergence ok\n");
    return 0;
}
