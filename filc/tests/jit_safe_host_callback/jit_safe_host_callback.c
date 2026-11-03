/* Host callbacks called from JITed code: the JITed function forwards its
   arguments to a mapped host function, which does the real work in host
   (pizlonated) code. Arguments cross the JIT boundary in both directions
   with full capability checking. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

static int hostSum(int* a, int n)
{
    int sum = 0;
    int i;
    for (i = 0; i < n; ++i)
        sum += a[i];
    return sum;
}

static __attribute__((noinline)) int (*buildFn(void))(int*, int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[2] = { ptrType, i32Type };
    LLVMTypeRef hostSumType = LLVMFunctionType(i32Type, paramTypes, 2, false);
    LLVMTypeRef functionType = hostSumType;
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMValueRef callee = LLVMAddFunction(module, "hostSum", hostSumType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef args[2] = { LLVMGetParam(function, 0), LLVMGetParam(function, 1) };
    LLVMValueRef sum = LLVMBuildCall2(builder, hostSumType, callee, args, 2, "sum");
    LLVMBuildRet(builder, sum);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "hostSum", hostSum);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int*, int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int*, int) = buildFn();
    int arr[10];
    int i;
    for (i = 0; i < 10; ++i)
        arr[i] = 100 + i;
    int n = (int)(intptr_t)opaque((void*)(intptr_t)10);
    int r = test((int*)opaque(arr), n);
    if (r != 1045) {
        fprintf(stderr, "sum %d, expected 1045\n", r);
        return 1;
    }

    printf("safe callback ok\n");
    return 0;
}
