/* Recursion in JITed code: the function computes n + (n-1) + ... + 1 by
   calling itself with a bounded depth of 100. JITed frames must behave like
   ordinary Fil-C frames across the recursion. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static __attribute__((noinline)) int (*buildFn(void))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef paramTypes[1] = { i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef base = LLVMAppendBasicBlockInContext(context, function, "base");
    LLVMBasicBlockRef recurse = LLVMAppendBasicBlockInContext(context, function, "recurse");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef n = LLVMGetParam(function, 0);
    LLVMValueRef isZero = LLVMBuildICmp(
        builder, LLVMIntEQ, n, LLVMConstInt(i32Type, 0, false), "iszero");
    LLVMBuildCondBr(builder, isZero, base, recurse);

    LLVMPositionBuilderAtEnd(builder, base);
    LLVMBuildRet(builder, LLVMConstInt(i32Type, 0, false));

    LLVMPositionBuilderAtEnd(builder, recurse);
    LLVMValueRef arg = LLVMBuildAdd(
        builder, n, LLVMConstInt(i32Type, -1, false), "arg");
    LLVMValueRef rest = LLVMBuildCall2(
        builder, functionType, function, &arg, 1, "rest");
    LLVMValueRef result = LLVMBuildAdd(builder, n, rest, "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int) = buildFn();
    int r = test(100);
    if (r != 5050) {
        fprintf(stderr, "recursion returned %d, expected 5050\n", r);
        return 1;
    }

    printf("safe recursion ok\n");
    return 0;
}
