#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildCall2(i32-fn-with-1-param, 0 args). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef paramTys[1];
    paramTys[0] = i32;
    LLVMTypeRef calleeTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                            paramTys, 1, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", calleeTy);
    LLVMBuildCall2(builder, calleeTy, callee, NULL, 0, "");
    return 0;
}
