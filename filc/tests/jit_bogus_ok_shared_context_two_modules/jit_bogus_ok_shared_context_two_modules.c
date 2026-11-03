#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Two modules built in one context before any compile: both compile through
   the same session and both functions return the right values. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);

    LLVMModuleRef module1 = LLVMModuleCreateWithNameInContext("test1", context);
    LLVMValueRef fn1 = LLVMAddFunction(module1, "test", i32Fn);
    LLVMBasicBlockRef entry1 = LLVMAppendBasicBlockInContext(context, fn1, "root");
    LLVMBuilderRef builder1 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder1, entry1);
    LLVMBuildRet(builder1, LLVMConstInt(i32, 11, false));

    LLVMModuleRef module2 = LLVMModuleCreateWithNameInContext("test2", context);
    LLVMValueRef fn2 = LLVMAddFunction(module2, "test", i32Fn);
    LLVMBasicBlockRef entry2 = LLVMAppendBasicBlockInContext(context, fn2, "root");
    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder2, entry2);
    LLVMBuildRet(builder2, LLVMConstInt(i32, 22, false));

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result1 = FilLLVMJIT_CompileModule(jit, module1);
    FilLLVMJITModuleRef result2 = FilLLVMJIT_CompileModule(jit, module2);
    int (*test1)(void) = FilLLVMJIT_GetPointerToGlobal(result1, "test");
    int (*test2)(void) = FilLLVMJIT_GetPointerToGlobal(result2, "test");
    if (!test1 || !test2 || test1() != 11 || test2() != 22) {
        fprintf(stderr, "bad: wrong results\n");
        abort();
    }

    printf("shared ctx ok\n");
    return 0;
}
