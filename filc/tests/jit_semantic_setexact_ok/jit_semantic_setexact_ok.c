#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetExact(sdiv) is legal. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot_a = LLVMBuildAlloca(builder, i32, "slot_a");
    LLVMValueRef a = LLVMBuildLoad2(builder, i32, slot_a, "a");
    LLVMValueRef slot_b = LLVMBuildAlloca(builder, i32, "slot_b");
    LLVMValueRef b = LLVMBuildLoad2(builder, i32, slot_b, "b");
    LLVMValueRef div = LLVMBuildSDiv(builder, a, b, "div");
    LLVMSetExact(div, true);
    LLVMValueRef r = LLVMBuildUDiv(builder, div, b, "r");
    LLVMBuildRet(builder, r);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("setexact ok\n");
    return 0;
}
