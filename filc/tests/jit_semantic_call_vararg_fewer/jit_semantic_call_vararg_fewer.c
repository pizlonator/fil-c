#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildCall2 with zero args on a vararg function type taking one fixed
   parameter.  CallInst::init (Instructions.cpp) asserts "Calling a function
   with bad signature!" when there are fewer args than fixed parameters, even
   for a vararg function type, so the wrapper's argument-count check must
   reject this: this is a Fil-C panic (exit 42) instead of that assertion
   abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef one = i32;
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, &one, 1, true);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fn_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildCall2(builder, fn_ty, fn, NULL, 0, "call");

    return 0;
}
