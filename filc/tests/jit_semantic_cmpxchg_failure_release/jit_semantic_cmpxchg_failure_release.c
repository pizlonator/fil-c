#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildAtomicCmpXchg with failure ordering=Release.  A cmpxchg cannot
   fail with a release-only ordering: AtomicCmpXchgInst::setFailureOrdering
   asserts "invalid CmpXchg failure ordering"
   (llvm/include/llvm/IR/Instructions.h) for both Release and AcquireRelease. */

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
    LLVMBuildAtomicCmpXchg(builder, slot, zero, zero,
                           LLVMAtomicOrderingMonotonic,
                           LLVMAtomicOrderingRelease, false);

    return 0;
}
