#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstShuffleVector(i32-const, i32-const, <4 x i32> mask). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef maskElems[4];
    maskElems[0] = LLVMConstInt(i32, 0, false);
    maskElems[1] = LLVMConstInt(i32, 1, false);
    maskElems[2] = LLVMConstInt(i32, 2, false);
    maskElems[3] = LLVMConstInt(i32, 3, false);
    LLVMConstShuffleVector(LLVMConstInt(i32, 0, false),
                           LLVMConstInt(i32, 1, false),
                           LLVMConstVector(maskElems, 4));
    return 0;
}
