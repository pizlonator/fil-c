#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the owner-aware LLVMSetOrdering check: a load, store,
   or fence keeps NotAtomic legal (only the atomicrmw owner restricts the
   ordering to the atomic set), and Monotonic is legal for all of them. */

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
    LLVMValueRef load = LLVMBuildLoad2(builder, i32, slot, "v");
    LLVMSetVolatile(load, true);
    LLVMSetOrdering(load, LLVMAtomicOrderingNotAtomic);
    LLVMSetOrdering(load, LLVMAtomicOrderingMonotonic);

    LLVMValueRef store = LLVMBuildStore(builder, LLVMConstInt(i32, 7, false),
                                        slot);
    LLVMSetOrdering(store, LLVMAtomicOrderingNotAtomic);

    LLVMValueRef fence = LLVMBuildFence(builder, LLVMAtomicOrderingNotAtomic,
                                        false, "");
    LLVMSetOrdering(fence, LLVMAtomicOrderingNotAtomic);
    /* The module verifier insists that a fence's ordering is one of
       acquire/release/acq_rel/seq_cst, so restore a legal one before
       compiling; the NotAtomic window above is the API-level control. */
    LLVMSetOrdering(fence, LLVMAtomicOrderingAcquire);

    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("setordering notatomic owners ok\n");
    return 0;
}
