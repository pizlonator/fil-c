/* The MT + GC regression net: 4 threads each compile 10 modules (their own
   contexts/modules/sessions, all dying inside the thread) and stash the
   function pointers. After the join, the main thread forces five collections
   with churn and then calls all 40 pointers — every one must still work and
   remember which thread and round produced it. */

#include <filc_llvm_jit.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define THREADS 4
#define PER_THREAD 10
#define TOTAL (THREADS * PER_THREAD)

static int (*fns[TOTAL])(void);

static __attribute__((noinline)) int (*buildOne(int id))(void)
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
                     (unsigned long long)(id * 100), false),
        LLVMConstInt(LLVMInt32TypeInContext(context), 7, false),
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
    for (i = 0; i < PER_THREAD; ++i)
        fns[tid * PER_THREAD + i] = buildOne(tid * PER_THREAD + i);
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
    for (k = 0; k < 5; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    int i;
    for (i = 0; i < TOTAL; ++i) {
        int r = fns[i]();
        if (r != i * 100 + 7) {
            fprintf(stderr, "fn %d returned %d\n", i, r);
            return 1;
        }
    }

    printf("mt gc ok\n");
    return 0;
}
