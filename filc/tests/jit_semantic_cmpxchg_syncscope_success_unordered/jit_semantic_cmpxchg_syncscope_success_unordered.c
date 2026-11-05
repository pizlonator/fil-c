#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildAtomicCmpXchgSyncScope with success ordering=Unordered.  The
   sync-scope wrapper shares the cmpxchg checks with LLVMBuildAtomicCmpXchg
   (both rows expand the same clause), and the same "invalid CmpXchg success
   ordering" assert (llvm/include/llvm/IR/Instructions.h) fires for it. */

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
    LLVMBuildAtomicCmpXchgSyncScope(builder, slot, zero, zero,
                                    LLVMAtomicOrderingUnordered,
                                    LLVMAtomicOrderingMonotonic, 0);

    return 0;
}
