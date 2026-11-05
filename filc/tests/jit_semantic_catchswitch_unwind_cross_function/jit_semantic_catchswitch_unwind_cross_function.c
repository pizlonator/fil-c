#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildCatchSwitch with an unwind destination that belongs to a
   different function.  A cross-function basic-block reference is only
   observable when CompileModule clones the module, and the clone asserts
   "Referenced value not in value map!" (ValueMapper.cpp) before any verifier
   runs, so the wrapper's same-function block check must reject it at build
   time: this is a Fil-C panic (exit 42) instead of that abort at compile
   time.  The parent pad is a null token constant, which is what the funclet
   donors use.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidTy = LLVMVoidTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMValueRef other = LLVMAddFunction(module, "other",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef otherBB = LLVMAppendBasicBlockInContext(context, other,
                                                              "obb");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef none = LLVMConstNull(LLVMTokenTypeInContext(context));
    LLVMBuildCatchSwitch(builder, none, otherBB, 1, "cs");

    return 0;
}
