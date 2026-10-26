#include <stdfil.h>
#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void foo(void)
{
    zprintf("Hello!\n");
}

static void* resolve(const char* name, void* arg)
{
    ZASSERT(!strcmp(name, "foo"));
    return foo;
}

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidFunctionType = LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", voidFunctionType);
    LLVMValueRef foo = LLVMAddFunction(module, "foo", voidFunctionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    /* FIXME: Add a variant of this test that passes NULL for the name argument. */
    LLVMBuildCall2(builder, voidFunctionType, foo, NULL, 0, "");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetResolveCallback(jit, resolve, NULL);
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module, "test");
    void (*test)(void) = FilLLVMJIT_GetFunctionAddress(result, "test");

    test();

    return 0;
}

