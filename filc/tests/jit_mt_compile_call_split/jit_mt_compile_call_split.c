/* Compile in one thread, call in others: one thread compiles a module through
   its own session and publishes the executable-module ref; after pthread_join
   (the happens-before edge), 4 threads concurrently do GetPointerToGlobal and
   call the JITed function 200 times each. The JITed function is pure, so
   concurrent calls are safe. */

#include <filc_llvm_jit.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define CALLERS 4
#define CALLS 200

static FilLLVMJITModuleRef compiledModule;

static void* compileThread(void* arg)
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
        builder, x, LLVMConstInt(LLVMInt32TypeInContext(context), 2, false), "r");
    result = LLVMBuildAdd(
        builder, result, LLVMConstInt(LLVMInt32TypeInContext(context), 1, false), "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    compiledModule = FilLLVMJIT_CompileModule(jit, module);
    if (!compiledModule)
        return (void*)(intptr_t)1;
    return NULL;
}

static void* callThread(void* arg)
{
    int seed = (int)(intptr_t)arg;
    int i;
    for (i = 0; i < CALLS; ++i) {
        int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiledModule, "test");
        if (!test)
            return (void*)(intptr_t)1;
        int r = test(seed + i);
        if (r != (seed + i) * 2 + 1)
            return (void*)(intptr_t)1;
    }
    return NULL;
}

int main()
{
    pthread_t compiler;
    if (pthread_create(&compiler, NULL, compileThread, NULL)) {
        fprintf(stderr, "pthread_create failed\n");
        return 1;
    }
    void* compilerRet = NULL;
    pthread_join(compiler, &compilerRet);
    if (compilerRet) {
        fprintf(stderr, "compile thread failed\n");
        return 1;
    }

    pthread_t threads[CALLERS];
    int t;
    for (t = 0; t < CALLERS; ++t) {
        if (pthread_create(threads + t, NULL, callThread, (void*)(intptr_t)(t * 1000))) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }
    for (t = 0; t < CALLERS; ++t) {
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

    printf("mt split ok\n");
    return 0;
}
