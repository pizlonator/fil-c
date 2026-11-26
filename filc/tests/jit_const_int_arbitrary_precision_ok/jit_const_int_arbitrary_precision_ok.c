#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstIntOfArbitraryPrecision(i200, 4 words) stays legal for a value
   that spans three words (the wrapper now snapshots the words instead of
   borrowing the caller's array), and the constant survives a JIT compile
   through a trunc to i32. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i200 = LLVMIntTypeInContext(context, 200);
    unsigned long long words[4] = { 0x9E3779B97F4A7C15ULL,
                                    0xFEDCBA9876543210ULL,
                                    0x0123456789ABCDEFULL,
                                    0 };
    LLVMValueRef value = LLVMConstIntOfArbitraryPrecision(i200, 4, words);
    if (!value) {
        fprintf(stderr, "bad: no constant\n");
        abort();
    }
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRet(builder, LLVMBuildTrunc(builder, value, i32, ""));

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }
    /* APInt words are little-endian, so the low 32 bits of word 0. */
    if (test() != (int)0x7f4a7c15u) {
        fprintf(stderr, "bad: wrong result: 0x%x\n", (unsigned)test());
        abort();
    }

    printf("const int arbitrary precision ok\n");
    return 0;
}
