#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildCondBr on an i32 condition instead of an i1.  BranchInst::AssertOK
   aborts (SIGABRT) with the boolean-predicates assertion. */

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
