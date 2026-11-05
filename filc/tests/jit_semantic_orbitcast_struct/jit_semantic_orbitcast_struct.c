#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildTruncOrBitCast(struct -> i32). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef fieldTy[1];
    fieldTy[0] = i32;
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTy, 1, false);
    LLVMValueRef slot = LLVMBuildAlloca(builder, structTy, "slot");
    LLVMValueRef s = LLVMBuildLoad2(builder, structTy, slot, "s");
    LLVMBuildTruncOrBitCast(builder, s, i32, "c");
    return 0;
}
