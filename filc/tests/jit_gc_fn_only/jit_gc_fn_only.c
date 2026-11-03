/* GC liveness: after the compile helper returns, the context, module, builder,
   session, and all LLVM values are dead; the ONLY thing kept alive is the
   JITed function pointer. Forcing collection with zgc_request_and_wait() plus
   allocation churn must not invalidate the code. This is the most basic shape
   of the verified GC-survival idiom. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static __attribute__((noinline)) int (*buildTest(void))(int)
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
        builder, x, LLVMConstInt(LLVMInt32TypeInContext(context), 3, false), "res");
    result = LLVMBuildAdd(
        builder, result, LLVMConstInt(LLVMInt32TypeInContext(context), 1, false), "res2");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
    /* context, module, builder, jit, and compiled all die here. */
}

int main()
{
    int (*test)(int) = buildTest();

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    int i;
    for (i = 0; i < 5; ++i) {
        int r = test(i + 1);
        if (r != (i + 1) * 3 + 1) {
            fprintf(stderr, "bad result %d for input %d\n", r, i + 1);
            return 1;
        }
    }

    printf("gc fn only ok\n");
    return 0;
}
