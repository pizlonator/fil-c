#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMSetInstrParamAlignment alignment check:
   alignment 16 (a non-zero power of two) stays legal.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnty = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                        &i32, 1, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", fnty);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef arg = LLVMConstInt(i32, 1, false);
    LLVMValueRef call = LLVMBuildCall2(builder, fnty, callee, &arg, 1, "");
    LLVMSetInstrParamAlignment(call, 1, 16);

    printf("setinstrparamalignment ok\n");
    return 0;
}
