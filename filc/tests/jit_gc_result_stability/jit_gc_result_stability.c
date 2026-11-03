/* GC must never change results: the JITed function computes a 20-term
   integer recurrence (v = v * 2 + 1, twenty times), and it is called 100
   times with forced collections interleaved. Every single call must return
   exactly the same value. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static __attribute__((noinline)) int (*buildTest(void))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef paramTypes[1] = { i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef value = LLVMGetParam(function, 0);
    int term;
    for (term = 0; term < 20; ++term) {
        value = LLVMBuildMul(
            builder, value, LLVMConstInt(i32Type, 2, false), "t");
        value = LLVMBuildAdd(
            builder, value, LLVMConstInt(i32Type, 1, false), "t");
    }
    LLVMBuildRet(builder, value);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

static int expected(int input)
{
    int value = input;
    int term;
    for (term = 0; term < 20; ++term)
        value = value * 2 + 1;
    return value;
}

int main()
{
    int (*test)(int) = buildTest();
    int want = expected(7);

    int i;
    for (i = 0; i < 100; ++i) {
        if (i % 5 == 4) {
            void* churn = zgc_alloc(1024 * 1024);
            (void)churn;
            zgc_request_and_wait();
        }
        int r = test(7);
        if (r != want) {
            fprintf(stderr, "call %d returned %d, expected %d\n", i, r, want);
            return 1;
        }
    }

    printf("gc stable ok\n");
    return 0;
}
