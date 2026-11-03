/* 8 threads, each with its own context/module/session: every thread compiles
   10 modules and calls each one, verifying results that encode the thread and
   round. Per-thread JIT state is the verified-solid MT pattern. All GC
   forcing stays on the main thread, after the join. */

#include <filc_llvm_jit.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define THREADS 8
#define ROUNDS 10

static __attribute__((noinline)) int (*buildOne(int salt))(int)
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
    LLVMValueRef result = LLVMBuildMul(
        builder, x, LLVMConstInt(LLVMInt32TypeInContext(context), 2, false), "r");
    result = LLVMBuildAdd(
        builder, result,
        LLVMConstInt(LLVMInt32TypeInContext(context), (unsigned long long)salt, false),
        "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

static void* threadMain(void* arg)
{
    int salt = (int)(intptr_t)arg * 1000;
    int round;
    for (round = 0; round < ROUNDS; ++round) {
        int (*test)(int) = buildOne(salt + round);
        int r = test(round);
        if (r != round * 2 + salt + round)
            return (void*)(intptr_t)1;
    }
    return NULL;
}

int main()
{
    pthread_t threads[THREADS];
    int t;
    for (t = 0; t < THREADS; ++t) {
        if (pthread_create(threads + t, NULL, threadMain, (void*)(intptr_t)t)) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }
    for (t = 0; t < THREADS; ++t) {
        void* ret = NULL;
        pthread_join(threads[t], &ret);
        if (ret) {
            fprintf(stderr, "thread %d failed\n", t);
            return 1;
        }
    }

    unsigned k;
    for (k = 0; k < 3; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    printf("mt own ok\n");
    return 0;
}
