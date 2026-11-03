/* GC liveness across two independently compiled modules: both contexts,
   modules, builders, and sessions die inside the compile helpers; only the
   two function pointers are kept. Interleaved forced GCs and calls into both
   modules must keep both working, with each module remembering which one it
   is. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static __attribute__((noinline)) int (*buildOne(
    const char* name, int multiplier))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext(name, context);
    LLVMTypeRef paramTypes[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef x = LLVMGetParam(function, 0);
    LLVMValueRef result = LLVMBuildMul(
        builder, x,
        LLVMConstInt(LLVMInt32TypeInContext(context), (unsigned long long)multiplier, false),
        "res");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*testA)(int) = buildOne("testA", 2);
    int (*testB)(int) = buildOne("testB", 5);

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();

        int ra = testA(10 + (int)k);
        int rb = testB(10 + (int)k);
        if (ra != (10 + (int)k) * 2) {
            fprintf(stderr, "round %u: A returned %d\n", k, ra);
            return 1;
        }
        if (rb != (10 + (int)k) * 5) {
            fprintf(stderr, "round %u: B returned %d\n", k, rb);
            return 1;
        }
    }

    printf("gc two modules ok\n");
    return 0;
}
