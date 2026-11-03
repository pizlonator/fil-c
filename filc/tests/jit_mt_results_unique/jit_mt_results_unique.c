/* Uniqueness across threads: 8 threads each compile 10 functions that return
   constants encoding threadID*1000 + i, call them, and record the results.
   After the join and forced GC, all 80 recorded results must be correct, all
   80 function pointers must still work, and no two results may collide. */

#include <filc_llvm_jit.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define THREADS 8
#define PER_THREAD 10
#define TOTAL (THREADS * PER_THREAD)

static int results[TOTAL];
static int (*fns[TOTAL])(void);

static __attribute__((noinline)) int (*buildOne(int value))(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef result = LLVMBuildAdd(
        builder,
        LLVMConstInt(LLVMInt32TypeInContext(context),
                     (unsigned long long)value, false),
        LLVMConstInt(LLVMInt32TypeInContext(context), 0, false),
        "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

static void* threadMain(void* arg)
{
    int tid = (int)(intptr_t)arg;
    int i;
    for (i = 0; i < PER_THREAD; ++i) {
        int (*test)(void) = buildOne(tid * 1000 + i);
        fns[tid * PER_THREAD + i] = test;
        results[tid * PER_THREAD + i] = test();
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

    int i;
    int j;
    for (i = 0; i < TOTAL; ++i) {
        int tid = i / PER_THREAD;
        int slot = i % PER_THREAD;
        if (results[i] != tid * 1000 + slot) {
            fprintf(stderr, "result %d is %d, expected %d\n",
                    i, results[i], tid * 1000 + slot);
            return 1;
        }
        if (fns[i]() != results[i]) {
            fprintf(stderr, "fn %d changed its answer after GC\n", i);
            return 1;
        }
    }
    for (i = 0; i < TOTAL; ++i) {
        for (j = i + 1; j < TOTAL; ++j) {
            if (results[i] == results[j]) {
                fprintf(stderr, "results %d and %d collide\n", i, j);
                return 1;
            }
        }
    }

    printf("mt unique ok\n");
    return 0;
}
