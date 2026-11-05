#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMBuildLandingPad nameable check: a landing
   pad with a void result type and an EMPTY name stays legal (setName("") is
   a no-op, so LLVM never asserts).
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
    LLVMValueRef lp = LLVMBuildLandingPad(builder, voidTy, fn, 0, "");
    if (!lp) {
        fprintf(stderr, "bad: no landing pad\n");
        abort();
    }

    printf("landingpad void no name ok\n");
    return 0;
}
