#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMCreateConstantRangeAttribute(range, 32 bits, [10, 100)) is legal. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    unsigned long long lower[1];
    unsigned long long upper[1];
    lower[0] = 10;
    upper[0] = 100;
    LLVMAttributeRef attr = LLVMCreateConstantRangeAttribute(
        context, 98, 32, lower, upper);
    LLVMTypeRef paramTys[1];
    paramTys[0] = i32;
    LLVMTypeRef fnTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                        paramTys, 1, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMAddAttributeAtIndex(fn, 1, attr);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("const range ok\n");
    return 0;
}