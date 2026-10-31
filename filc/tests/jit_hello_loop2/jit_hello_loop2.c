#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>

static bool didRun = false;
static unsigned i;

static void foo(char* str)
{
    ZASSERT(!didRun);
    ZASSERT(!strcmp(str, zasprintf("yeah %u", i)));
    didRun = true;
}

int main()
{
    for (i = 1000; i--;) {
        LLVMContextRef context = LLVMContextCreate();
        LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
        char* str = zasprintf("yeah %u", i);
        LLVMValueRef stringConst = LLVMConstStringInContext(context, str, strlen(str), false);
        LLVMValueRef stringGlobal = LLVMAddGlobal(module, LLVMTypeOf(stringConst), "string");
        LLVMSetInitializer(stringGlobal, stringConst);
        LLVMTypeRef voidFunctionType = LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false);
        LLVMValueRef function = LLVMAddFunction(module, "test", voidFunctionType);
        LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
        LLVMTypeRef voidStrFunctionType = LLVMFunctionType(LLVMVoidTypeInContext(context), &ptrType, 1, false);
        LLVMValueRef fooValue = LLVMAddFunction(module, "foo", voidStrFunctionType);
        LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
        LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
        LLVMPositionBuilderAtEnd(builder, rootBlock);
        LLVMBuildCall2(builder, voidStrFunctionType, fooValue, &stringGlobal, 1, "");
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

