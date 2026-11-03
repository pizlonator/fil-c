#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* After compiling module 1, a second module created in the same context can
   still be compiled through the same session, and both results are callable.
   (The "context moves to the JIT" doc language is more restrictive than the
   actual behavior.) */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);
    FilLLVMJITRef jit = FilLLVMJIT_Create();

    LLVMModuleRef module1 = LLVMModuleCreateWithNameInContext("test1", context);
    LLVMValueRef fn1 = LLVMAddFunction(module1, "test", i32Fn);
    LLVMBasicBlockRef entry1 = LLVMAppendBasicBlockInContext(context, fn1, "root");
    LLVMBuilderRef builder1 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder1, entry1);
    LLVMBuildRet(builder1, LLVMConstInt(i32, 1, false));
    FilLLVMJITModuleRef result1 = FilLLVMJIT_CompileModule(jit, module1);
    int (*test1)(void) = FilLLVMJIT_GetPointerToGlobal(result1, "test");
    if (!test1 || test1() != 1) {
        fprintf(stderr, "bad: first module\n");
        abort();
    }

    LLVMModuleRef module2 = LLVMModuleCreateWithNameInContext("test2", context);
    LLVMValueRef fn2 = LLVMAddFunction(module2, "test", i32Fn);
    LLVMBasicBlockRef entry2 = LLVMAppendBasicBlockInContext(context, fn2, "root");
    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder2, entry2);
    LLVMBuildRet(builder2, LLVMConstInt(i32, 2, false));
    FilLLVMJITModuleRef result2 = FilLLVMJIT_CompileModule(jit, module2);
    int (*test2)(void) = FilLLVMJIT_GetPointerToGlobal(result2, "test");
    if (!test2 || test2() != 2) {
        fprintf(stderr, "bad: second module\n");
        abort();
    }

    printf("reuse context ok\n");
    return 0;
}
