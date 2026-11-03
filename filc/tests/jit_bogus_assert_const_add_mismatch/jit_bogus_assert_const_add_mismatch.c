#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstAdd over an i32 and an i64 constant.  Constant folding requires
   both operands to have the same type; the assertion in Constants.cpp aborts
   (SIGABRT). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);

    LLVMValueRef a = LLVMConstInt(i32, 1, false);
    LLVMValueRef b = LLVMConstInt(i64, 2, false);
    LLVMConstAdd(a, b);

    return 0;
}
