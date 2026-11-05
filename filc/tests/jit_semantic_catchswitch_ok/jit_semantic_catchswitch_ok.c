#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMBuildCatchSwitch handler-count check: a
   catchswitch created with one reserved handler stays legal, takes a
   same-function AddHandler on top, and takes a catchpad nested in it (the
   module is not compiled, matching the funclet donors: compiling verifiable
   EH IR would need a whole funclet web, which is not what this control is
   about). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidTy = LLVMVoidTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef handlerBB = LLVMAppendBasicBlockInContext(context, fn,
                                                                "handler");
    LLVMBasicBlockRef next = LLVMAppendBasicBlockInContext(context, fn, "next");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef none = LLVMConstNull(LLVMTokenTypeInContext(context));

    LLVMValueRef cs = LLVMBuildCatchSwitch(builder, none, next, 1, "cs");
    LLVMAddHandler(cs, handlerBB);

    LLVMPositionBuilderAtEnd(builder, handlerBB);
    LLVMValueRef pad = LLVMBuildCatchPad(builder, cs, NULL, 0, "");
    LLVMBuildCatchRet(builder, pad, next);

    LLVMPositionBuilderAtEnd(builder, next);
    LLVMBuildRetVoid(builder);

    printf("catchswitch with handler ok\n");
    return 0;
}
