#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetArgOperand(catchpad, 5). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef none = LLVMConstNull(LLVMTokenTypeInContext(context));
    LLVMValueRef catchSwitch = LLVMBuildCatchSwitch(builder, none, entry, 1, "cs");
    LLVMValueRef catchPad = LLVMBuildCatchPad(builder, catchSwitch, NULL, 0, "cp");
    LLVMGetArgOperand(catchPad, 5);
    return 0;
}
