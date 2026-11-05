#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildAddrSpaceCast(ptr0 -> ptr1) is legal. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef ptr0 = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef ptr1 = LLVMPointerTypeInContext(context, 1);
    LLVMValueRef slot = LLVMBuildAlloca(builder, ptr0, "slot");
    LLVMValueRef p = LLVMBuildLoad2(builder, ptr0, slot, "p");
    LLVMBuildAddrSpaceCast(builder, p, ptr1, "c");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("addrspacecast ok\n");
    return 0;
}