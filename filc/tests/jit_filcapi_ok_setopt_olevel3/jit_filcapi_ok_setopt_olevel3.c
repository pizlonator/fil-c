/* FilLLVMJIT_SetOption("opt-level", "3") before compilation: the shim maps
   this to OptimizationLevel::O3 / CodeGenOptLevel::Aggressive. Aggressive
   optimization of the Fil-C pass pipeline plus codegen must not change the
   semantics of the module. SetOption must be called on the session before
   FilLLVMJIT_CompileModule. */

#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

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
    FilLLVMJIT_SetOption(jit, "opt-level", "3");

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

    printf("setopt olevel3 ok\n");
    return 0;
}
