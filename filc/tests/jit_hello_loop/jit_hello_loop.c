#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>

static bool didRun = false;

static void foo(void)
{
    ZASSERT(!didRun);
    didRun = true;
}

int main()
{
    for (unsigned i = 1000; i--;) {
        LLVMContextRef context = LLVMContextCreate();
        LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
        LLVMTypeRef voidFunctionType = LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
        LLVMValueRef function = LLVMAddFunction(module, "test", voidFunctionType);
        LLVMValueRef fooValue = LLVMAddFunction(module, "foo", voidFunctionType);
        LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
        LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
        LLVMPositionBuilderAtEnd(builder, rootBlock);
        LLVMBuildCall2(builder, voidFunctionType, fooValue, NULL, 0, "");
        LLVMBuildRetVoid(builder);
        
        FilLLVMJITRef jit = FilLLVMJIT_Create();
        FilLLVMJIT_SetGlobalMapping(jit, "foo", foo);
        FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
        void (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");

        ZASSERT(!didRun);
        test();
        ZASSERT(didRun);
        didRun = false;
    }

    return 0;
}

