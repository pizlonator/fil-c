#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstPtrToInt(i32-const, i32). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMConstPtrToInt(LLVMConstInt(i32, 1, false), i32);
    return 0;
}
