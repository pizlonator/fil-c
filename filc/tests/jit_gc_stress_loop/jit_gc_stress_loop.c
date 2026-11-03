/* The jit_hello_bad1 GC-race scenario, looped: each round compiles a fresh
   module whose JITed function calls a host callee with a module string global
   (the loop3/bad1 shape), calls it, drops everything but the function
   pointer, and forces a collection. 300 rounds with a verified counter; any
   fugc/JIT rooting regression shows up here.
   (bad1 itself sets LLVMInternalLinkage on the callee declaration, which
   makes the module verifier reject the module — that is what bad1 pins. The
   GC-race scenario itself needs the valid external-callee module used here.) */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>
#include <string.h>

#define ROUNDS 300

static bool didRun = false;
static unsigned i;

static void foo(char* str)
{
    if (didRun)
        abort();
    if (strcmp(str, zasprintf("yeah %u", i)))
        abort();
    didRun = true;
}

static __attribute__((noinline)) void (*buildTest(void))(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    char* str = zasprintf("yeah %u", i);
    LLVMValueRef stringConst = LLVMConstStringInContext(
        context, str, (unsigned)strlen(str), false);
    LLVMValueRef stringGlobal = LLVMAddGlobal(
        module, LLVMTypeOf(stringConst), "string");
    LLVMSetInitializer(stringGlobal, stringConst);
    LLVMTypeRef voidFunctionType = LLVMFunctionType(
        LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", voidFunctionType);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef voidStrFunctionType = LLVMFunctionType(
        LLVMVoidTypeInContext(context), &ptrType, 1, false);
    LLVMValueRef fooValue = LLVMAddFunction(module, "foo", voidStrFunctionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMBuildCall2(builder, voidStrFunctionType, fooValue, &stringGlobal, 1, "");
    LLVMBuildRetVoid(builder);
    /* NOTE: jit_hello_bad1 sets LLVMInternalLinkage on this declaration —
       that makes the module verifier reject it (that test pins the verifier
       panic). The GC-race *scenario* here is loop3's: external callee. */

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "foo", foo);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    void (*test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    for (i = 0; i < ROUNDS; ++i) {
        void (*test)(void) = buildTest();

        zgc_request_and_wait();

        didRun = false;
        test();
        if (!didRun)
            abort();
    }

    printf("gc stress ok\n");
    return 0;
}
