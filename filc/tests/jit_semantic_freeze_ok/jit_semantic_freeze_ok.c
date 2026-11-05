#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMBuildFreeze nameable check: a freeze of a
   non-void value with a non-empty name stays legal.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(i32, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef fz = LLVMBuildFreeze(builder, LLVMConstInt(i32, 5, false),
                                      "fz");
    if (!fz) {
        fprintf(stderr, "bad: no freeze\n");
        abort();
    }
    LLVMBuildRet(builder, fz);

    printf("freeze ok\n");
    return 0;
}
