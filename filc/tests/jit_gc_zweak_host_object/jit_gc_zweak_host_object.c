/* Experimental: a host object mapped into a JIT session must be rooted by the
   session (zweak stays live while the session does), and after the whole
   compile side is dropped the object must become collectable (the zweak goes
   dead). The host object is created, mapped, and dropped entirely inside a
   noinline helper so no stale stack words or registers can conservatively
   keep it alive after the helper returns; only the zweak (rooted via a
   global) outlives it. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static zweak* theWeak;

static __attribute__((noinline)) void alivePhase(void)
{
    int* hostObj = zgc_alloc(sizeof(int));
    *hostObj = 1234;
    theWeak = zweak_new(hostObj);

    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMValueRef hostObjGlobal = LLVMAddGlobal(module, i32Type, "hostObj");
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef loaded = LLVMBuildLoad2(builder, i32Type, hostObjGlobal, "v");
    LLVMBuildRet(builder, loaded);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "hostObj", hostObj);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
        if (!zweak_get(theWeak)) {
            fprintf(stderr, "mapped object collected while session alive (round %u)\n", k);
            exit(1);
        }
    }
    if (test() != 1234) {
        fprintf(stderr, "JITed read returned %d\n", test());
        exit(1);
    }

    /* jit, compiled, module, context, builder, and hostObj all die here. */
}

int main()
{
    alivePhase();

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    if (zweak_get(theWeak)) {
        fprintf(stderr, "dropped mapped object was not collected\n");
        return 1;
    }

    printf("gc zweak ok\n");
    return 0;
}
