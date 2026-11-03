/* GC liveness for a host-callee mapping: the session is dropped right after
   the compile, only the JITed function pointer is kept, and after forced GC
   the JITed code must still reach the mapped host function. The mapping must
   outlive the session object that created it. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static int hostSum(int a, int b)
{
    return a + b;
}

static __attribute__((noinline)) int (*buildTest(void))(int, int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef paramTypes[2] = {
        LLVMInt32TypeInContext(context),
        LLVMInt32TypeInContext(context)
    };
    LLVMTypeRef hostSumType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 2, false);
    LLVMTypeRef functionType = hostSumType;
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMValueRef hostSumValue = LLVMAddFunction(module, "hostSum", hostSumType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef args[2] = { LLVMGetParam(function, 0), LLVMGetParam(function, 1) };
    LLVMValueRef sum = LLVMBuildCall2(
        builder, hostSumType, hostSumValue, args, 2, "sum");
    LLVMBuildRet(builder, sum);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "hostSum", hostSum);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int, int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
    /* jit dies here; the compiled code and its mapping must survive. */
}

int main()
{
    int (*test)(int, int) = buildTest();

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    if (test(20, 22) != 42) {
        fprintf(stderr, "bad sum 20+22\n");
        return 1;
    }
    if (test(-5, 47) != 42) {
        fprintf(stderr, "bad sum -5+47\n");
        return 1;
    }
    if (test(0, 0) != 0) {
        fprintf(stderr, "bad sum 0+0\n");
        return 1;
    }

    printf("gc mapping ok\n");
    return 0;
}
