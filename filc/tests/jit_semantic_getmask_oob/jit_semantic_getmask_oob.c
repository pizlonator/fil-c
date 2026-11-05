#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetMaskValue(shuffle, 9) with a 4-element mask. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef vec = LLVMVectorType(i32, 4);
    LLVMValueRef vslot = LLVMBuildAlloca(builder, vec, "vslot");
    LLVMValueRef v1 = LLVMBuildLoad2(builder, vec, vslot, "v1");
    LLVMValueRef v2 = LLVMBuildLoad2(builder, vec, vslot, "v2");
    LLVMValueRef maskElems[4];
    maskElems[0] = LLVMConstInt(i32, 0, false);
    maskElems[1] = LLVMConstInt(i32, 1, false);
    maskElems[2] = LLVMConstInt(i32, 2, false);
    maskElems[3] = LLVMConstInt(i32, 3, false);
    LLVMValueRef shuffle = LLVMBuildShuffleVector(builder, v1, v2,
                                                  LLVMConstVector(maskElems, 4),
                                                  "s");
    LLVMGetMaskValue(shuffle, 9);
    return 0;
}
