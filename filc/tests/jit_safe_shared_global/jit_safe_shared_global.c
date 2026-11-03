/* A host int global shared with JITed code through a global mapping: the
   JITed function increments it and returns the new value; the host reads the
   same storage. Both sides must agree after every call. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static int counter = 5;

static __attribute__((noinline)) int (*buildFn(void))(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMValueRef counterGlobal = LLVMAddGlobal(module, i32Type, "counter");
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef loaded = LLVMBuildLoad2(builder, i32Type, counterGlobal, "c");
    LLVMValueRef incremented = LLVMBuildAdd(
        builder, loaded, LLVMConstInt(i32Type, 1, false), "c1");
    LLVMBuildStore(builder, incremented, counterGlobal);
    LLVMBuildRet(builder, incremented);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "counter", &counter);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(void) = buildFn();

    int r1 = test();
    int r2 = test();
    if (r1 != 6 || r2 != 7) {
        fprintf(stderr, "bad results %d %d\n", r1, r2);
        return 1;
    }
    if (counter != 7) {
        fprintf(stderr, "host sees counter = %d\n", counter);
        return 1;
    }

    printf("safe shared ok\n");
    return 0;
}
