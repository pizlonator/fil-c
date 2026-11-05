#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the cmpxchg ordering checks: every success ordering in
   {Monotonic, Acquire, Release, AcquireRelease, SequentiallyConsistent} with
   every failure ordering in {Monotonic, Acquire, SequentiallyConsistent}
   must stay legal, and a cmpxchg with matching comparand types must reach
   the wrapper's LLVM call. */

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
    LLVMAtomicOrdering success[5] = {
        LLVMAtomicOrderingMonotonic,
        LLVMAtomicOrderingAcquire,
        LLVMAtomicOrderingRelease,
        LLVMAtomicOrderingAcquireRelease,
        LLVMAtomicOrderingSequentiallyConsistent
    };
    LLVMAtomicOrdering failure[3] = {
        LLVMAtomicOrderingMonotonic,
        LLVMAtomicOrderingAcquire,
        LLVMAtomicOrderingSequentiallyConsistent
    };
    unsigned s;
    unsigned f;
    for (s = 0; s < 5; ++s) {
        for (f = 0; f < 3; ++f) {
            LLVMBuildAtomicCmpXchg(builder, slot, zero, zero,
                                   success[s], failure[f], false);
            LLVMBuildAtomicCmpXchgSyncScope(builder, slot, zero, zero,
                                            success[s], failure[f], 0);
        }
    }
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("cmpxchg orderings ok\n");
    return 0;
}
