#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildCallBr with a non-empty name on a void callee: the callbr's result
   value is void, and naming a void value aborts in LLVM (Value.cpp). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef calleeTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                            NULL, 0, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", calleeTy);
    LLVMBasicBlockRef defaultBB = LLVMAppendBasicBlockInContext(context, fn,
                                                                "default");
    LLVMBasicBlockRef destBB = LLVMAppendBasicBlockInContext(context, fn,
                                                             "dest");
    LLVMBuildCallBr(builder, calleeTy, callee, defaultBB, &destBB, 1, NULL, 0,
                    NULL, 0, "cb");
    return 0;
}
