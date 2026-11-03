/* Concurrent GetPointerToGlobal on distinct symbols of one precompiled
   module: 4 threads each look up their own function 200 times and call it,
   verifying results. Lookup of distinct names in one executable module is the
   verified-safe concurrent-access pattern. */

#include <filc_llvm_jit.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define THREADS 4
#define ROUNDS 200

static FilLLVMJITModuleRef compiledModule;

static void* buildThread(void* arg)
{
    (void)arg;
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef paramTypes[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 1, false);
    int f;
    for (f = 0; f < THREADS; ++f) {
        char name[16];
        snprintf(name, sizeof(name), "test%d", f);
        LLVMValueRef function = LLVMAddFunction(module, name, functionType);
        LLVMBasicBlockRef rootBlock =
            LLVMAppendBasicBlockInContext(context, function, "root");
        LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
        LLVMPositionBuilderAtEnd(builder, rootBlock);
        LLVMValueRef x = LLVMGetParam(function, 0);
        LLVMValueRef result = LLVMBuildAdd(
            builder, x,
            LLVMConstInt(LLVMInt32TypeInContext(context),
                         (unsigned long long)(f * 1000), false),
            "r");
        LLVMBuildRet(builder, result);
    }

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    compiledModule = FilLLVMJIT_CompileModule(jit, module);
    if (!compiledModule)
        return (void*)(intptr_t)1;
    return NULL;
}

static void* callThread(void* arg)
{
    int tid = (int)(intptr_t)arg;
    char name[16];
    snprintf(name, sizeof(name), "test%d", tid);
    int i;
    for (i = 0; i < ROUNDS; ++i) {
        int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiledModule, name);
        if (!test)
            return (void*)(intptr_t)1;
        int r = test(5 + i);
        if (r != 5 + i + tid * 1000)
            return (void*)(intptr_t)1;
    }
    return NULL;
}

int main()
{
    pthread_t builder;
    if (pthread_create(&builder, NULL, buildThread, NULL)) {
        fprintf(stderr, "pthread_create failed\n");
        return 1;
    }
    void* builderRet = NULL;
    pthread_join(builder, &builderRet);
    if (builderRet) {
        fprintf(stderr, "build thread failed\n");
        return 1;
    }

    pthread_t threads[THREADS];
    int t;
    for (t = 0; t < THREADS; ++t) {
        if (pthread_create(threads + t, NULL, callThread, (void*)(intptr_t)t)) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }
    for (t = 0; t < THREADS; ++t) {
        void* ret = NULL;
        pthread_join(threads[t], &ret);
        if (ret) {
            fprintf(stderr, "call thread %d failed\n", t);
            return 1;
        }
    }

    unsigned k;
    for (k = 0; k < 3; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    printf("mt getptr ok\n");
    return 0;
}
