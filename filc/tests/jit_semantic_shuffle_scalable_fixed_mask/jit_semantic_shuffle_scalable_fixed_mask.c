#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildShuffleVector over two <vscale x 2 x i32> values with a fixed
   non-uniform mask [0, 1].  A scalable source requires a uniform mask of all
   zeros or all poison: ShuffleVectorInst::isValidOperands asserts ("Invalid
   shuffle vector instruction operands!", Instructions.cpp); the wrapper's
   mask check must reject it first. */

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
    LLVMTypeRef svec = LLVMScalableVectorType(i32, 2);
    LLVMValueRef vslot = LLVMBuildAlloca(builder, svec, "vslot");
    LLVMValueRef v1 = LLVMBuildLoad2(builder, svec, vslot, "v1");
    LLVMValueRef v2 = LLVMBuildLoad2(builder, svec, vslot, "v2");
    LLVMValueRef maskElems[2];
    maskElems[0] = LLVMConstInt(i32, 0, false);
    maskElems[1] = LLVMConstInt(i32, 1, false);
    LLVMValueRef mask = LLVMConstVector(maskElems, 2);
    LLVMBuildShuffleVector(builder, v1, v2, mask, "s");
    return 0;
}
