#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* An atomic load is a legal shape for both sync-scope entry points: the
   getter returns the scope id and the setter changes it, and the module
   compiles. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn,
                                                            "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef load = LLVMBuildLoad2(builder, i32, slot, "v");
    LLVMSetOrdering(load, LLVMAtomicOrderingMonotonic);
    if (LLVMGetAtomicSyncScopeID(load) != 1) {
        fprintf(stderr, "bad: unexpected initial sync scope id\n");
        abort();
    }
    LLVMSetAtomicSyncScopeID(load, 2);
    if (LLVMGetAtomicSyncScopeID(load) != 2) {
        fprintf(stderr, "bad: sync scope id did not stick\n");
        abort();
    }
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("atomic sync scope ok\n");
    return 0;
}
