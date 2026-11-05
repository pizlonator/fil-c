#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMBuildFreeze nameable check: a freeze of a
   void-typed value with an EMPTY name stays legal (setName("") is a no-op,
   so LLVM never asserts).
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidTy = LLVMVoidTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef fz = LLVMBuildFreeze(builder, LLVMGetUndef(voidTy), "");
    if (!fz) {
        fprintf(stderr, "bad: no freeze\n");
        abort();
    }
    LLVMBuildRetVoid(builder);

    printf("freeze void no name ok\n");
    return 0;
}
