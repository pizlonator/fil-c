#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildGEP2 may claim an i64 source element type over an i32* slot: with
   opaque pointers the IR is well-formed and CompileModule accepts it. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef indices[1] = { LLVMConstInt(i64, 0, false) };
    LLVMBuildGEP2(builder, i64, slot, indices, 1, "gep");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    void (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    test();

    printf("gep lie ok\n");
    return 0;
}
