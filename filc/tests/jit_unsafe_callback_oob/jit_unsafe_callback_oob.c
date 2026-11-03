/* An out-of-bounds write reached through a JITed callback: the JITed function
   calls a mapped host function and hands it a host array; the host function
   writes way past the end. The panic happens in host (pizlonated) code that
   was reached from JITed code, and must still be
   "cannot write pointer with ptr >= upper." with exit 42. The index is
   laundered through opaque() so the host-side check cannot be folded. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

static void hostPanics(int* p)
{
    int i = (int)(intptr_t)opaque((void*)(intptr_t)100);
    /* p[i] with i = 100 is 400 bytes past the start of the 8-byte array. */
    p[i] = 42;
}

static __attribute__((noinline)) void (*buildFn(void))(int*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[1] = { ptrType };
    LLVMTypeRef hostPanicsType = LLVMFunctionType(
        LLVMVoidTypeInContext(context), paramTypes, 1, false);
    LLVMTypeRef functionType = hostPanicsType;
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMValueRef callee = LLVMAddFunction(module, "hostPanics", hostPanicsType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef args[1] = { LLVMGetParam(function, 0) };
    LLVMBuildCall2(builder, hostPanicsType, callee, args, 1, "");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "hostPanics", hostPanics);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    void (*test)(int*) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    void (*test)(int*) = buildFn();
    int arr[2] = { 111, 222 };
    test((int*)opaque(arr));
    fprintf(stderr, "should have panicked\n");
    return 1;
}
