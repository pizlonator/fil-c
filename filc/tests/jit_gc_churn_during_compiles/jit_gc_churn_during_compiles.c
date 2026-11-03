/* GC running between compiles: 100 rounds of (fresh context+module+session,
   compile, drop, forced GC), with every compiled function pointer kept and
   every one of them called and verified at the end. This is the
   jit_hello_loop3 shape extended to keep all of the pointers instead of just
   the last one. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

#define ROUNDS 100

static __attribute__((noinline)) int (*buildOne(int round))(int)
{
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
        builder, x,
        LLVMConstInt(LLVMInt32TypeInContext(context), (unsigned long long)(round + 1), false),
        "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    static int (*tests[ROUNDS])(int);
    int i;
    for (i = 0; i < ROUNDS; ++i) {
        tests[i] = buildOne(i);
        zgc_request_and_wait();
    }

    for (i = 0; i < ROUNDS; ++i) {
        int r = tests[i](10);
        if (r != 10 * (i + 1)) {
            fprintf(stderr, "round %d function returned %d\n", i, r);
            return 1;
        }
    }

    printf("gc churn compiles ok\n");
    return 0;
}
