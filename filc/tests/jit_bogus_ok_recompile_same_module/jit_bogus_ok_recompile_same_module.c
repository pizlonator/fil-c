#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Compiling the same module twice works despite the header's "consumes the
   module" language: both compilations yield callable function pointers with
   correct results. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", i32Fn);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRet(builder, LLVMConstInt(i32, 7, false));

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result1 = FilLLVMJIT_CompileModule(jit, module);
    int (*test1)(void) = FilLLVMJIT_GetPointerToGlobal(result1, "test");
    if (!test1 || test1() != 7) {
        fprintf(stderr, "bad: first compile\n");
        abort();
    }

    FilLLVMJITModuleRef result2 = FilLLVMJIT_CompileModule(jit, module);
    int (*test2)(void) = FilLLVMJIT_GetPointerToGlobal(result2, "test");
    if (!test2 || test2() != 7) {
        fprintf(stderr, "bad: second compile\n");
        abort();
    }

    printf("recompile ok\n");
    return 0;
}
