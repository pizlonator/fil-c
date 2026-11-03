#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Bogus OR #3: LLVMBuildOr over two loaded {i32,i32} aggregates.  LLVM's
   BinaryOperator::AssertOK aborts (SIGABRT) with the non-integral-type
   assertion before any compilation. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fields[2] = { i32, i32 };
    LLVMTypeRef st = LLVMStructTypeInContext(context, fields, 2, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, st, "slot");
    LLVMValueRef agg = LLVMBuildLoad2(builder, st, slot, "v");
    LLVMBuildOr(builder, agg, agg, "or");

    return 0;
}
