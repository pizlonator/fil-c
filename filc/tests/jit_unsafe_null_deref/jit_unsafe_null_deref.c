/* A NULL pointer passed into JITed code must still be checked when the JITed
   function loads through it: "cannot read pointer with null object." and
   exit 42. The JITed function receives the null as an ordinary parameter. */

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
    LLVMValueRef loaded = LLVMBuildLoad2(builder, i32Type, p, "v");
    LLVMBuildRet(builder, loaded);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int*) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int*) = buildFn();
    int* p = (int*)opaque(NULL);
    test(p);
    fprintf(stderr, "should have panicked\n");
    return 1;
}
