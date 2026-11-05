#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildCondBr on an i32 condition instead of an i1.  The wrapper's
   condition check rejects it, so this is a Fil-C panic (exit 42) instead of
   BranchInst::AssertOK's boolean-predicates assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef thenBB = LLVMAppendBasicBlockInContext(context, fn, "then");
    LLVMBasicBlockRef elseBB = LLVMAppendBasicBlockInContext(context, fn, "else");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef cond = LLVMConstInt(i32, 1, false);
    LLVMBuildCondBr(builder, cond, thenBB, elseBB);

    return 0;
}
