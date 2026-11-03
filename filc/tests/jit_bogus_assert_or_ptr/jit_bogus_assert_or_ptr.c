#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Bogus OR #2: LLVMBuildOr over two loaded pointers.  LLVM's
   BinaryOperator::AssertOK aborts (SIGABRT) with the non-integral-type
   assertion before any compilation. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef ptrT = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, ptrT, "slot");
    LLVMValueRef lw = LLVMBuildLoad2(builder, ptrT, slot, "v");
    LLVMBuildOr(builder, lw, lw, "or");

    return 0;
}
