#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Bogus OR #1: LLVMBuildOr over two float loads.  The runtime wrapper does
   not check operand types, so LLVM's own BinaryOperator::AssertOK aborts
   (SIGABRT) before any compilation can happen. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, flt, "slot");
    LLVMValueRef lwf = LLVMBuildLoad2(builder, flt, slot, "v");
    LLVMBuildOr(builder, lwf, lwf, "or");

    return 0;
}
