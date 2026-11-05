#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMAddGlobalIFunc address-space check: address
   space 2^24-1 (the largest value the 24-bit SubclassData field holds) stays
   legal.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef resolver = LLVMConstInt(i32, 0, false);
    LLVMValueRef ifunc = LLVMAddGlobalIFunc(module, "if", 2, fnty, 0xffffff,
                                            resolver);
    if (!ifunc) {
        fprintf(stderr, "bad: no ifunc\n");
        abort();
    }

    printf("addglobalifunc as ok\n");
    return 0;
}
