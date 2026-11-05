#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildShuffleVector(<4 x i32>, <4 x float>, <4 x i32> mask). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef vec = LLVMVectorType(i32, 4);
    LLVMValueRef vslot = LLVMBuildAlloca(builder, vec, "vslot");
    LLVMValueRef v1 = LLVMBuildLoad2(builder, vec, vslot, "v1");
    LLVMTypeRef vecF = LLVMVectorType(flt, 4);
    LLVMValueRef fslot = LLVMBuildAlloca(builder, vecF, "fslot");
    LLVMValueRef v2 = LLVMBuildLoad2(builder, vecF, fslot, "v2");
    LLVMValueRef maskElems[4];
    maskElems[0] = LLVMConstInt(i32, 0, false);
    maskElems[1] = LLVMConstInt(i32, 1, false);
    maskElems[2] = LLVMConstInt(i32, 2, false);
    maskElems[3] = LLVMConstInt(i32, 3, false);
    LLVMValueRef mask = LLVMConstVector(maskElems, 4);
    LLVMBuildShuffleVector(builder, v1, v2, mask, "s");
    return 0;
}
