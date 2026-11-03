/* GC liveness for a data pointer: the only thing kept alive after the compile
   helper returns is a pointer to a module global holding a function pointer
   (the jit_hello_loop4 shape). The data symbol must survive forced GC, and
   the function it points at must stay callable. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>
#include <string.h>

static bool didRun = false;
static const char* theString = "gc data ptr target";

static void foo(char* str)
{
    if (didRun || strcmp(str, theString)) {
        fprintf(stderr, "bad foo call\n");
        exit(1);
    }
    didRun = true;
}

static __attribute__((noinline)) void (**buildTest(void))(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef stringConst = LLVMConstStringInContext(
        context, theString, (unsigned)strlen(theString), false);
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
    LLVMSetLinkage(function, LLVMInternalLinkage);

    LLVMValueRef functionPtrGlobal = LLVMAddGlobal(
        module, LLVMPointerTypeInContext(context, 0), "testPtr");
    LLVMSetInitializer(functionPtrGlobal, function);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "foo", foo);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    void (**test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "testPtr");
    return test;
}

int main()
{
    void (**test)(void) = buildTest();

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    int i;
    for (i = 0; i < 5; ++i) {
        didRun = false;
        (*test)();
        if (!didRun) {
            fprintf(stderr, "call %d did not run\n", i);
            return 1;
        }
    }

    printf("gc data ptr ok\n");
    return 0;
}
