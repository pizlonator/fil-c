#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildShuffleVector over two <2 x i32> values with mask element 100.
   ShuffleVectorInst::isValidOperands asserts ("Invalid shuffle vector
   instruction operands!", Instructions.cpp); the wrapper's mask check must
   reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn,
                                                            "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef vec = LLVMVectorType(i32, 2);
    LLVMValueRef vslot = LLVMBuildAlloca(builder, vec, "vslot");
    LLVMValueRef v1 = LLVMBuildLoad2(builder, vec, vslot, "v1");
    LLVMValueRef v2 = LLVMBuildLoad2(builder, vec, vslot, "v2");
    LLVMValueRef maskElems[2];
    maskElems[0] = LLVMConstInt(i32, 0, false);
    maskElems[1] = LLVMConstInt(i32, 100, false);
    LLVMValueRef mask = LLVMConstVector(maskElems, 2);
    LLVMBuildShuffleVector(builder, v1, v2, mask, "s");
    return 0;
}
