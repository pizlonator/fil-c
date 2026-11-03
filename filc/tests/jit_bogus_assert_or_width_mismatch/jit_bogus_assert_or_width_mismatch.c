#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Bogus OR #4: LLVMBuildOr over an i32 load and an i64 load.  Both are
   integral, so BinaryOperator::AssertOK fails one assertion later: the
   operands must have the same type.  SIGABRT before any compilation. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot32 = LLVMBuildAlloca(builder, i32, "slot32");
    LLVMValueRef slot64 = LLVMBuildAlloca(builder, i64, "slot64");
    LLVMValueRef lw32 = LLVMBuildLoad2(builder, i32, slot32, "v32");
    LLVMValueRef lw64 = LLVMBuildLoad2(builder, i64, slot64, "v64");
    LLVMBuildOr(builder, lw32, lw64, "or");

    return 0;
}
