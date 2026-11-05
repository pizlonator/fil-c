#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMSetParamAlignment alignment check:
   alignment 16 (a non-zero power of two) stays legal.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnty = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                        &i32, 1, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnty);
    LLVMValueRef arg = LLVMGetFirstParam(fn);
    LLVMSetParamAlignment(arg, 16);

    printf("setparamalignment ok\n");
    return 0;
}
