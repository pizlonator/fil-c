/* GC liveness across many modules: 100 tiny modules are compiled in a loop,
   all contexts/modules/sessions die each round, and all 100 function pointers
   are kept. After five forced collections every one of the 100 functions must
   still work and still remember which module it came from. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define COUNT 100

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
        LLVMConstInt(LLVMInt32TypeInContext(context), (unsigned long long)(id * 1000), false),
        LLVMConstInt(LLVMInt32TypeInContext(context), 42, false),
        "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    static int (*tests[COUNT])(void);
    int i;
    for (i = 0; i < COUNT; ++i)
        tests[i] = buildOne(i);

    unsigned k;
    for (k = 0; k < 5; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    for (i = 0; i < COUNT; ++i) {
        int r = tests[i]();
        if (r != i * 1000 + 42) {
            fprintf(stderr, "module %d returned %d\n", i, r);
            return 1;
        }
    }

    printf("gc many ok\n");
    return 0;
}
