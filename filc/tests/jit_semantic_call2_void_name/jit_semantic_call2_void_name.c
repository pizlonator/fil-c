#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildCall2 with a non-empty name on a void callee: the call's result
   value is void, and naming a void value aborts in LLVM (Value.cpp). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef calleeTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                            NULL, 0, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", calleeTy);
    LLVMBuildCall2(builder, calleeTy, callee, NULL, 0, "cal");
    return 0;
}
