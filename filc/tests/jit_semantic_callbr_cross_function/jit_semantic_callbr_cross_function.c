#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* CallBr in fn test with default-BB of fn other. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidTy = LLVMVoidTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMValueRef other = LLVMAddFunction(module, "other",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef otherEntry = LLVMAppendBasicBlockInContext(context, other, "root");
    LLVMBasicBlockRef next = LLVMAppendBasicBlockInContext(context, fn, "next");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef calleeTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                            NULL, 0, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", calleeTy);
    LLVMBuildCallBr(builder, calleeTy, callee, otherEntry, NULL, 0, NULL, 0,
                    NULL, 0, "");
    return 0;
}
