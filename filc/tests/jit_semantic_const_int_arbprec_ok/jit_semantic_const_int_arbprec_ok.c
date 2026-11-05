#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstIntOfArbitraryPrecision(i32, 3 words) is legal. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    unsigned long long words[3];
    words[0] = 7;
    words[1] = 0;
    words[2] = 0;
    LLVMValueRef v = LLVMConstIntOfArbitraryPrecision(i32, 3, words);
    LLVMBuildRet(builder, v);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test || test() != 7) {
        fprintf(stderr, "bad: wrong result\n");
        abort();
    }

    printf("const int arbprec ok\n");
    return 0;
}
