#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildICmp(EQ, ptr, ptr) is legal. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef pa = LLVMBuildLoad2(builder, ptrTy, slot, "pa");
    LLVMValueRef pb = LLVMBuildLoad2(builder, ptrTy, slot, "pb");
    LLVMValueRef eq = LLVMBuildICmp(builder, LLVMIntEQ, pa, pb, "eq");
    LLVMValueRef asI32 = LLVMBuildZExt(builder, eq, i32, "asI32");
    LLVMBuildRet(builder, asI32);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test || test() != 1) {
        fprintf(stderr, "bad: wrong result\n");
        abort();
    }

    printf("icmp ptr ok\n");
    return 0;
}
