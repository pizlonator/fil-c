#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildExtractValue(struct-load, 0) is legal. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef fieldTys[2];
    fieldTys[0] = i32;
    fieldTys[1] = i32;
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 2, false);
    LLVMValueRef slot = LLVMBuildAlloca(builder, structTy, "slot");
    LLVMValueRef s = LLVMBuildLoad2(builder, structTy, slot, "s");
    LLVMValueRef e = LLVMBuildExtractValue(builder, s, 0, "e");
    LLVMBuildRet(builder, e);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test || test() != 0) {
        fprintf(stderr, "bad: wrong result\n");
        abort();
    }

    printf("extractvalue ok\n");
    return 0;
}
