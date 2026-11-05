#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMVectorType(struct, 2). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef fieldTys[2];
    fieldTys[0] = i32;
    fieldTys[1] = flt;
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 2, false);
    LLVMVectorType(structTy, 2);
    return 0;
}
