#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMSetCmpXchgFailureOrdering with ordering=AcquireRelease.  See
   jit_semantic_setcmpxchg_failure_release. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef zero = LLVMConstInt(i32, 0, false);
    LLVMValueRef cx = LLVMBuildAtomicCmpXchg(builder, slot, zero, zero,
                                             LLVMAtomicOrderingMonotonic,
                                             LLVMAtomicOrderingMonotonic,
                                             false);
    LLVMSetCmpXchgFailureOrdering(cx, LLVMAtomicOrderingAcquireRelease);

    return 0;
}
