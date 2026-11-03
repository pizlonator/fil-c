/* Mixed MT stress: 8 threads, 20 rounds each. Threads 0-3 compile and call
   their own modules with their own contexts and sessions; threads 4-7
   concurrently do GetPointerToGlobal on one precompiled shared module and
   call it. The two halves never share a context or builder; the shared module
   is only ever read concurrently. */

#include <filc_llvm_jit.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define THREADS 8
#define COMPILERS 4
#define ROUNDS 20

static FilLLVMJITModuleRef sharedModule;

static void* buildShared(void* arg)
{
    (void)arg;
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
        builder, x, LLVMConstInt(LLVMInt32TypeInContext(context), 3, false), "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    sharedModule = FilLLVMJIT_CompileModule(jit, module);
    if (!sharedModule)
        return (void*)(intptr_t)1;
    return NULL;
}

static void* compilerThread(void* arg)
{
    int tid = (int)(intptr_t)arg;
    int round;
    for (round = 0; round < ROUNDS; ++round) {
        LLVMContextRef context = LLVMContextCreate();
        LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
        LLVMTypeRef functionType = LLVMFunctionType(
            LLVMInt32TypeInContext(context), NULL, 0, false);
        LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
        LLVMBasicBlockRef rootBlock =
            LLVMAppendBasicBlockInContext(context, function, "root");
        LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
        LLVMPositionBuilderAtEnd(builder, rootBlock);
        LLVMValueRef result = LLVMBuildAdd(
            builder,
            LLVMConstInt(LLVMInt32TypeInContext(context),
                         (unsigned long long)(tid * 100 + round), false),
            LLVMConstInt(LLVMInt32TypeInContext(context), 1, false),
            "r");
        LLVMBuildRet(builder, result);

        FilLLVMJITRef jit = FilLLVMJIT_Create();
        FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
        int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
        if (!test || test() != tid * 100 + round + 1)
            return (void*)(intptr_t)1;
    }
    return NULL;
}

static void* callerThread(void* arg)
{
    int tid = (int)(intptr_t)arg;
    int round;
    for (round = 0; round < ROUNDS; ++round) {
        int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(sharedModule, "test");
        if (!test)
            return (void*)(intptr_t)1;
        int r = test(tid + round);
        if (r != (tid + round) * 3)
            return (void*)(intptr_t)1;
    }
    return NULL;
}

int main()
{
    pthread_t builder;
    if (pthread_create(&builder, NULL, buildShared, NULL)) {
        fprintf(stderr, "pthread_create failed\n");
        return 1;
    }
    void* builderRet = NULL;
    pthread_join(builder, &builderRet);
    if (builderRet) {
        fprintf(stderr, "shared build failed\n");
        return 1;
    }

    pthread_t threads[THREADS];
    int t;
    for (t = 0; t < THREADS; ++t) {
        void* (*start)(void*) = (t < COMPILERS) ? compilerThread : callerThread;
        if (pthread_create(threads + t, NULL, start, (void*)(intptr_t)t)) {
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

    printf("mt stress ok\n");
    return 0;
}
