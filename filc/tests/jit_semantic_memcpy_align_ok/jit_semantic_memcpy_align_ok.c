#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the mem-builder alignment checks: zero (unspecified)
   and power-of-two alignments stay legal, and the built memcpy/memmove/memset
   survive a compile. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef dst = LLVMBuildAlloca(builder, i32, "d");
    LLVMValueRef src = LLVMBuildAlloca(builder, i32, "s");
    LLVMBuildMemCpy(builder, dst, 8, src, 0, LLVMConstInt(LLVMInt64TypeInContext(context), 4, false));
    LLVMBuildMemMove(builder, dst, 0, src, 16, LLVMConstInt(LLVMInt64TypeInContext(context), 4, false));
    LLVMBuildMemSet(builder, dst, LLVMConstInt(LLVMInt8TypeInContext(context), 0, false),
                    LLVMConstInt(LLVMInt64TypeInContext(context), 4, false),
                    0);
    LLVMBuildRetVoid(builder);
    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }
    printf("memcpy align ok\n");
    return 0;
}
