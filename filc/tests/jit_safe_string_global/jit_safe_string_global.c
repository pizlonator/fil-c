/* A module's internal string global read from JITed code: the JITed function
   returns the string's first byte, and the host verifies it. Module data
   symbols are readable from JITed code with full checking. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>
#include <string.h>

static const char* theString = "safe string global";

static __attribute__((noinline)) int (*buildFn(void))(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef stringConst = LLVMConstStringInContext(
        context, theString, (unsigned)strlen(theString), false);
    LLVMValueRef stringGlobal = LLVMAddGlobal(
        module, LLVMTypeOf(stringConst), "msg");
    LLVMSetInitializer(stringGlobal, stringConst);
    LLVMSetLinkage(stringGlobal, LLVMInternalLinkage);

    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef zero = LLVMConstInt(i32Type, 0, false);
    LLVMValueRef first = LLVMBuildInBoundsGEP2(
        builder, i8Type, stringGlobal, &zero, 1, "p");
    LLVMValueRef byte = LLVMBuildLoad2(builder, i8Type, first, "b");
    LLVMValueRef result = LLVMBuildZExt(builder, byte, i32Type, "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(void) = buildFn();
    int r = test();
    if (r != (unsigned char)'s') {
        fprintf(stderr, "first byte %d, expected %d\n", r, (unsigned char)'s');
        return 1;
    }

    printf("safe string ok\n");
    return 0;
}
