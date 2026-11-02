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

    /* LLVMMDNodeInContext treats a raw value element as function-local
       metadata, which Core.cpp only permits as the single element
       ("Expected only one operand to function-local metadata"); two raw
       instructions must be rejected.  (The operands are loads so that the
       builder does not constant-fold the adds away.)  This must panic in
       the wrapper instead of tripping an LLVM assertion. */
    LLVMValueRef vals[2] = { LLVMBuildAdd(builder, lhs, rhs, "sum1"),
                             LLVMBuildAdd(builder, lhs, rhs, "sum2") };
    LLVMMDNodeInContext(context, vals, 2);

    return 0;
}
