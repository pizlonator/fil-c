#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNamedStruct(st{i32,flt}, [flt, i32], 2). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef fieldTys[2];
    fieldTys[0] = i32;
    fieldTys[1] = flt;
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 2, false);
    LLVMValueRef elems[2];
    elems[0] = LLVMConstReal(flt, 1.0);
    elems[1] = LLVMConstInt(i32, 1, false);
    LLVMConstNamedStruct(structTy, elems, 2);
    return 0;
}
