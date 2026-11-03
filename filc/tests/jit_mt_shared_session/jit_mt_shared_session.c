/* One shared JIT session used concurrently: 4 threads each map their own
   distinct host-callee name into the shared session and compile 5 modules
   through it, then call and verify. The runtime's session/context locks
   serialize the API surface; every thread uses its own context and module.
   GC forcing stays on the main thread after the join. */

#include <filc_llvm_jit.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define THREADS 4
#define MODULES 5

static FilLLVMJITRef sharedJit;

static int hostAdd(int a, int b)
{
    return a + b;
}

static __attribute__((noinline)) int (*buildOne(const char* calleeName))(int, int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef paramTypes[2] = {
        LLVMInt32TypeInContext(context),
        LLVMInt32TypeInContext(context)
    };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 2, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMValueRef callee = LLVMAddFunction(module, calleeName, functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef args[2] = { LLVMGetParam(function, 0), LLVMGetParam(function, 1) };
    LLVMValueRef sum = LLVMBuildCall2(builder, functionType, callee, args, 2, "sum");
    LLVMBuildRet(builder, sum);

    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(sharedJit, module);
    int (*test)(int, int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

static void* threadMain(void* arg)
{
    int tid = (int)(intptr_t)arg;
    char calleeName[32];
    snprintf(calleeName, sizeof(calleeName), "hostAdd%d", tid);
    FilLLVMJIT_SetGlobalMapping(sharedJit, calleeName, hostAdd);

    int i;
    for (i = 0; i < MODULES; ++i) {
        int (*test)(int, int) = buildOne(calleeName);
        if (test(tid, 40) != tid + 40)
            return (void*)(intptr_t)1;
    }
    return NULL;
}

int main()
{
    sharedJit = FilLLVMJIT_Create();

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

    printf("mt shared session ok\n");
    return 0;
}
