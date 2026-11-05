#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstTruncOrBitCast(struct-const, i32). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef fieldTys[2];
    fieldTys[0] = i32;
    fieldTys[1] = flt;
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 2, false);
    LLVMConstTruncOrBitCast(LLVMConstNull(structTy), i32);
    return 0;
}
