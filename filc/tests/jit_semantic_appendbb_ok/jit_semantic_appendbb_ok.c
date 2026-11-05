#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAppendExistingBasicBlock with a parent-less basic block
   (LLVMCreateBasicBlockInContext makes one) is legal: the block is moved into
   the function's block list. */

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
    LLVMAppendExistingBasicBlock(fn, orphan);
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

    printf("appendbb ok\n");
    return 0;
}
