#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* SetInitializer(global-i32, i32-const) is legal. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef g = LLVMAddGlobal(module, i32, "g");
    LLVMSetInitializer(g, LLVMConstInt(i32, 42, false));
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(i32, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef v = LLVMBuildLoad2(builder, i32, g, "v");
    LLVMBuildRet(builder, v);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test || test() != 42) {
        fprintf(stderr, "bad: wrong result\n");
        abort();
    }

    printf("setinitializer ok\n");
    return 0;
}
