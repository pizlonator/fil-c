#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMSetCmpXchgSuccessOrdering and
   LLVMSetCmpXchgFailureOrdering checks: the full legal sets (success in
   {Monotonic, Acquire, Release, AcquireRelease, SequentiallyConsistent},
   failure in {Monotonic, Acquire, SequentiallyConsistent}) must stay legal. */

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
    for (s = 0; s < 5; ++s)
        LLVMSetCmpXchgSuccessOrdering(cx, success[s]);
    for (f = 0; f < 3; ++f)
        LLVMSetCmpXchgFailureOrdering(cx, failure[f]);

    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("set cmpxchg orderings ok\n");
    return 0;
}
