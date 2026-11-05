#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* MoveBasicBlockBefore(BB-of-other, BB-of-test). */

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
    LLVMMoveBasicBlockBefore(otherEntry, next);
    return 0;
}
