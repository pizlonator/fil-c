#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstShuffleVector over two <2 x i32> constants with mask element
   100.  ConstantExpr::getShuffleVector asserts
   ShuffleVectorInst::isValidOperands ("Invalid shuffle vector constant expr
   operands!", Constants.cpp); the wrapper's mask check must reject it
   first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef vec = LLVMVectorType(i32, 2);
    LLVMValueRef maskElems[2];
    maskElems[0] = LLVMConstInt(i32, 0, false);
    maskElems[1] = LLVMConstInt(i32, 100, false);
    LLVMValueRef mask = LLVMConstVector(maskElems, 2);
    LLVMConstShuffleVector(LLVMConstNull(vec), LLVMConstNull(vec), mask);
    return 0;
}
