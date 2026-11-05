#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMFunctionType(i32, [label], 1) is legal (LLVM-lenient). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef params[1];
    params[0] = LLVMLabelTypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, params, 1, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    if (!fn) {
        fprintf(stderr, "bad: no function\n");
        abort();
    }

    printf("fntype label ok\n");
    return 0;
}
