#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMInsertIntoBuilderWithName of a detached void-typed instruction with
   a non-empty name.  IRBuilderBase::Insert names the instruction after
   inserting it, and Value::setNameImpl (Value.cpp) asserts "Cannot assign a
   name to void values!", so the wrapper's nameable check must reject it:
   this is a Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

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
    /* A freeze built with no insert position is created detached. */
    LLVMBuilderRef detached = LLVMCreateBuilderInContext(context);
    LLVMValueRef inst = LLVMBuildFreeze(detached, LLVMGetUndef(voidTy), "");
    LLVMInsertIntoBuilderWithName(builder, inst, "name");

    return 0;
}
