/* GC liveness for a shared host global: the JITed function increments a host
   int through a global mapping, and GC is forced between calls. The mapped
   object must stay put and the counter must strictly increase across
   collections. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static int counter = 100;

static __attribute__((noinline)) int (*buildTest(void))(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMValueRef counterGlobal = LLVMAddGlobal(module, i32Type, "counter");
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef loaded = LLVMBuildLoad2(builder, i32Type, counterGlobal, "c");
    LLVMValueRef incremented = LLVMBuildAdd(
        builder, loaded, LLVMConstInt(i32Type, 1, false), "c1");
    LLVMBuildStore(builder, incremented, counterGlobal);
    LLVMBuildRet(builder, incremented);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "counter", &counter);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(void) = buildTest();

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();

        int r = test();
        if (r != 101 + (int)k) {
            fprintf(stderr, "round %u: bad counter %d, expected %u\n",
                    k, r, 101 + k);
            return 1;
        }
        if (counter != r) {
            fprintf(stderr, "round %u: host sees %d, JITed returned %d\n",
                    k, counter, r);
            return 1;
        }
    }

    printf("gc shared global ok\n");
    return 0;
}
