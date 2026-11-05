#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMInsertExistingBasicBlockAfterInsertBlock with a parent-less basic block
   (LLVMCreateBasicBlockInContext makes one) and a positioned builder is
   legal: the block is spliced in after the insert position. */

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
    LLVMBuildRetVoid(builder);
    LLVMBasicBlockRef orphan = LLVMCreateBasicBlockInContext(context, "orphan");
    LLVMInsertExistingBasicBlockAfterInsertBlock(builder, orphan);
    LLVMPositionBuilderAtEnd(builder, orphan);
    LLVMBuildRetVoid(builder);

    if (LLVMCountBasicBlocks(fn) != 2) {
        fprintf(stderr, "bad: expected two blocks\n");
        abort();
    }

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("insertbb ok\n");
    return 0;
}
