#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMSetOrdering on an atomicrmw with ordering=NotAtomic.  The wrapper's
   ordering check is owner-aware: an llvm::AtomicRMWInst rejects it, matching
   AtomicRMWInst::setOrdering's "atomicrmw instructions can only be atomic."
   assert (llvm/include/llvm/IR/Instructions.h). */

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
    LLVMValueRef val = LLVMConstInt(i32, 1, false);
    LLVMValueRef rmw = LLVMBuildAtomicRMW(builder, LLVMAtomicRMWBinOpXchg, slot,
                                          val, LLVMAtomicOrderingMonotonic,
                                          false);
    LLVMSetOrdering(rmw, LLVMAtomicOrderingNotAtomic);

    return 0;
}
