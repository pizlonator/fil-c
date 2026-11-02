#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef functionType = LLVMFunctionType(int32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef slotA = LLVMBuildAlloca(builder, int32Type, "a");
    LLVMValueRef slotB = LLVMBuildAlloca(builder, int32Type, "b");
    LLVMValueRef lhs = LLVMBuildLoad2(builder, int32Type, slotA, "x");
    LLVMValueRef rhs = LLVMBuildLoad2(builder, int32Type, slotB, "y");
    LLVMValueRef sum = LLVMBuildAdd(builder, lhs, rhs, "sum");

    /* LLVMGetNumIndices demands a llvm::GEPOperator, llvm::ExtractValueInst,
       or llvm::InsertValueInst (Core.cpp ends in an llvm_unreachable for
       anything else, which aborts even in NDEBUG builds); an add instruction
       is none of those.  This must panic in the wrapper instead of
       tripping LLVM's llvm_unreachable. */
    LLVMGetNumIndices(sum);

    return 0;
}
