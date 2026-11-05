#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the atomicrmw ordering check: every ordering in
   {Monotonic, Acquire, Release, AcquireRelease, SequentiallyConsistent} must
   stay legal (the check only rejects NotAtomic and Unordered), and the
   atomicrmw must actually reach the wrapper's LLVM call. */

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
    LLVMAtomicOrdering orderings[5] = {
        LLVMAtomicOrderingMonotonic,
        LLVMAtomicOrderingAcquire,
        LLVMAtomicOrderingRelease,
        LLVMAtomicOrderingAcquireRelease,
        LLVMAtomicOrderingSequentiallyConsistent
    };
    unsigned i;
    for (i = 0; i < 5; ++i) {
        LLVMBuildAtomicRMW(builder, LLVMAtomicRMWBinOpAdd, slot, val,
                           orderings[i], false);
        LLVMBuildAtomicRMWSyncScope(builder, LLVMAtomicRMWBinOpAdd, slot, val,
                                    orderings[i], 0);
    }
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("atomicrmw orderings ok\n");
    return 0;
}
