/* One session, one host mapping, concurrent compiles: the main thread maps a
   host callee into the session before any thread starts; 4 threads then
   compile 5 distinct modules each through the shared session, all calling the
   mapped host function. Compiles into a shared session are serialized by the
   runtime's session lock. */

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

static void* threadMain(void* arg)
{
    int tid = (int)(intptr_t)arg;
    int i;
    for (i = 0; i < MODULES; ++i) {
        LLVMContextRef context = LLVMContextCreate();
        LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
        LLVMTypeRef paramTypes[2] = {
            LLVMInt32TypeInContext(context),
            LLVMInt32TypeInContext(context)
        };
        LLVMTypeRef functionType = LLVMFunctionType(
            LLVMInt32TypeInContext(context), paramTypes, 2, false);
        LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
        LLVMValueRef callee = LLVMAddFunction(module, "hostAdd", functionType);
        LLVMBasicBlockRef rootBlock =
            LLVMAppendBasicBlockInContext(context, function, "root");
        LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
        LLVMPositionBuilderAtEnd(builder, rootBlock);
        LLVMValueRef args[2] = {
            LLVMConstInt(LLVMInt32TypeInContext(context), (unsigned long long)tid, false),
            LLVMConstInt(LLVMInt32TypeInContext(context), 40, false)
        };
        LLVMValueRef sum = LLVMBuildCall2(
            builder, functionType, callee, args, 2, "sum");
        LLVMBuildRet(builder, sum);

        FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(sharedJit, module);
        int (*test)(int, int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
        if (!test || test(tid, 40) != tid + 40)
            return (void*)(intptr_t)1;
    }
    return NULL;
}

int main()
{
    sharedJit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(sharedJit, "hostAdd", hostAdd);

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

    printf("mt mapping ok\n");
    return 0;
}
