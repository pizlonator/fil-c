#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildPtrDiff2 over pointers in different address spaces.  With opaque
   pointers that is the reachable same-type mismatch, and IRBuilderBase::
   CreatePtrDiff asserts LHS->getType() == RHS->getType() ("Pointer
   subtraction operand types must match!", IRBuilder.cpp); the wrapper's
   same-type check must reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef p0 = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef p1 = LLVMPointerTypeInContext(context, 1);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn,
                                                            "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef lhs = LLVMBuildLoad2(builder, p0, slot, "lhs");
    LLVMValueRef rhs = LLVMBuildIntToPtr(
        builder, LLVMConstNull(LLVMInt64TypeInContext(context)), p1, "rhs");
    LLVMBuildPtrDiff2(builder, i32, lhs, rhs, "d");
    return 0;
}
