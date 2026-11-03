/* Dropping a JITed module after use must not poison later compiles: compile,
   call, then drop the executable-module ref and everything else, force GC,
   and then compile a fresh module and call it. Both functions must work; the
   first one keeps working after its module handle is gone. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static __attribute__((noinline)) int (*buildAndCall(int input, int* result))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef paramTypes[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef x = LLVMGetParam(function, 0);
    LLVMValueRef sum = LLVMBuildAdd(
        builder, x, LLVMConstInt(LLVMInt32TypeInContext(context), 1000, false), "r");
    LLVMBuildRet(builder, sum);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    *result = test(input);
    /* compiled (the executable-module ref) and jit die here, but test
       keeps pointing at the code. */
    return test;
}

int main()
{
    int firstResult = 0;
    int (*first)(int) = buildAndCall(1, &firstResult);
    if (firstResult != 1001) {
        fprintf(stderr, "first call returned %d\n", firstResult);
        return 1;
    }

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    /* The dropped module's code must still run after GC. */
    if (first(2) != 1002) {
        fprintf(stderr, "dropped module call returned %d\n", first(2));
        return 1;
    }

    /* And a fresh compile must work just fine, too. */
    int secondResult = 0;
    int (*second)(int) = buildAndCall(5, &secondResult);
    if (secondResult != 1005) {
        fprintf(stderr, "second call returned %d\n", secondResult);
        return 1;
    }
    for (k = 0; k < 3; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }
    if (first(3) != 1003 || second(6) != 1006) {
        fprintf(stderr, "post-GC calls went sideways: %d %d\n", first(3), second(6));
        return 1;
    }

    printf("gc drop ok\n");
    return 0;
}
