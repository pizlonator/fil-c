/* A NULL pointer is fine to pass into JITed code as long as the JITed code
   does not dereference it: the function compares the pointer against null and
   returns 8 for null (the comparison flag + 7), 7 otherwise. Passing null
   capabilities across the JIT boundary is legal. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

static __attribute__((noinline)) int (*buildFn(void))(int*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[1] = { ptrType };
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef p = LLVMGetParam(function, 0);
    LLVMValueRef isNull = LLVMBuildICmp(
        builder, LLVMIntEQ, p, LLVMConstNull(ptrType), "isnull");
    LLVMValueRef flag = LLVMBuildZExt(builder, isNull, i32Type, "flag");
    LLVMValueRef result = LLVMBuildAdd(
        builder, flag, LLVMConstInt(i32Type, 7, false), "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int*) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int*) = buildFn();
    int* p = (int*)opaque(NULL);
    if (test(p) != 8) {
        fprintf(stderr, "null check returned %d\n", test(p));
        return 1;
    }
    int x = 99;
    if (test((int*)opaque(&x)) != 7) {
        fprintf(stderr, "non-null check returned %d\n", test(&x));
        return 1;
    }

    printf("safe null ok\n");
    return 0;
}
