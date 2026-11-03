/* Out-of-bounds WRITE through a JITed function: storing past the end of a
   host array must trip the capability check in the JITed code
   ("cannot write pointer with ptr >= upper.") and exit 42. The index is
   opaque-laundered and passed as a parameter. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

static __attribute__((noinline)) void (*buildFn(void))(int*, int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[2] = { ptrType, i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMVoidTypeInContext(context), paramTypes, 2, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef a = LLVMGetParam(function, 0);
    LLVMValueRef i = LLVMGetParam(function, 1);
    LLVMValueRef elemPtr = LLVMBuildGEP2(builder, i32Type, a, &i, 1, "p");
    LLVMBuildStore(builder, LLVMConstInt(i32Type, 12345, false), elemPtr);
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    void (*test)(int*, int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    void (*test)(int*, int) = buildFn();
    int arr[2] = { 111, 222 };
    int i = (int)(intptr_t)opaque((void*)(intptr_t)9);
    /* arr[i] with i = 9 is 36 bytes past the start of the 8-byte array. */
    test(arr, i);
    fprintf(stderr, "should have panicked\n");
    return 1;
}
