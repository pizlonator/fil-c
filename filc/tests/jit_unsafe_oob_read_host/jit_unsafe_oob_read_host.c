/* The pizlonator pass runs on JITed code unconditionally, so an
   out-of-bounds read through a JITed function must hit the same capability
   check as host code: "cannot read pointer with ptr >= upper." and exit 42.
   The index is laundered through opaque() and passed as a parameter so
   nothing can fold the check away (a constant small overrun can hide inside
   allocation slack, but a runtime index cannot). */

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
    int i = (int)(intptr_t)opaque((void*)(intptr_t)4);
    /* arr[i] with i = 4 is 8 bytes past the end of the 8-byte array. */
    test(arr, i);
    fprintf(stderr, "should have panicked\n");
    return 1;
}
