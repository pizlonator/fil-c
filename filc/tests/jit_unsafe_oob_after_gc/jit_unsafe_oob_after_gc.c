/* The JITed code's memory-safety checks must survive garbage collection:
   after three forced collections (with churn), an out-of-bounds read through
   a JITed function still panics with "cannot read pointer with ptr >= upper."
   and exit 42. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

static __attribute__((noinline)) int (*buildFn(void))(int*, int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[2] = { ptrType, i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, paramTypes, 2, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef a = LLVMGetParam(function, 0);
    LLVMValueRef i = LLVMGetParam(function, 1);
    LLVMValueRef elemPtr = LLVMBuildGEP2(builder, i32Type, a, &i, 1, "p");
    LLVMValueRef loaded = LLVMBuildLoad2(builder, i32Type, elemPtr, "v");
    LLVMBuildRet(builder, loaded);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int*, int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int*, int) = buildFn();
    int arr[2] = { 111, 222 };

    /* In-bounds reads work before and after GC. */
    int i = (int)(intptr_t)opaque((void*)(intptr_t)1);
    if (test(arr, i) != 222) {
        fprintf(stderr, "in-bounds read broke\n");
        return 1;
    }
    unsigned k;
    for (k = 0; k < 3; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }
    if (test(arr, i) != 222) {
        fprintf(stderr, "in-bounds read broke after GC\n");
        return 1;
    }

    int bad = (int)(intptr_t)opaque((void*)(intptr_t)4);
    test(arr, bad);
    fprintf(stderr, "should have panicked\n");
    return 1;
}
