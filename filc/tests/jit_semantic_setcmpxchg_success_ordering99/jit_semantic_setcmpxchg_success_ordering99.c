#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetCmpXchgSuccessOrdering(cmpxchg, 99). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef p = LLVMBuildLoad2(builder, ptrTy, slot, "p");
    LLVMValueRef cx = LLVMBuildAtomicCmpXchg(
        builder, p, LLVMConstInt(i32, 0, false), LLVMConstInt(i32, 1, false),
        LLVMAtomicOrderingMonotonic, LLVMAtomicOrderingMonotonic, false);
    LLVMSetCmpXchgSuccessOrdering(cx, 99);
    return 0;
}
