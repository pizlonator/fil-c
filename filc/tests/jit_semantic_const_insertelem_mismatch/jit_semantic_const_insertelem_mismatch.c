#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstInsertElement(<4 x i32>, float-const, i32-const). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMValueRef elems[4];
    elems[0] = LLVMConstInt(i32, 0, false);
    elems[1] = LLVMConstInt(i32, 1, false);
    elems[2] = LLVMConstInt(i32, 2, false);
    elems[3] = LLVMConstInt(i32, 3, false);
    LLVMValueRef vec = LLVMConstVector(elems, 4);
    LLVMConstInsertElement(vec, LLVMConstReal(flt, 1.0),
                           LLVMConstInt(i32, 0, false));
    return 0;
}
