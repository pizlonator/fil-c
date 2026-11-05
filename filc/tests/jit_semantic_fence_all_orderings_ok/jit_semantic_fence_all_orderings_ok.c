#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildFence with every ordering that is legal for a fence end-to-end:
   Acquire (4), Release (5), AcquireRelease (6), and SequentiallyConsistent (7)
   (the verifier rejects Monotonic fences at CompileModule).  Release was
   falsely rejected by the old sem_is_ordering mask. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildFence(builder, LLVMAtomicOrderingAcquire, false, "");
    LLVMBuildFence(builder, LLVMAtomicOrderingRelease, false, "");
    LLVMBuildFence(builder, LLVMAtomicOrderingAcquireRelease, false, "");
    LLVMBuildFence(builder, LLVMAtomicOrderingSequentiallyConsistent, false,
                   "");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("fence orderings ok\n");
    return 0;
}
