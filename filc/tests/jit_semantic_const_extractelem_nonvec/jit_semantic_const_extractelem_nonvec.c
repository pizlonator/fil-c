#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstExtractElement(i32-const, i32-const). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMConstExtractElement(LLVMConstInt(i32, 1, false),
                            LLVMConstInt(i32, 0, false));
    return 0;
}
