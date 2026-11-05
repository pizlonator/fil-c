#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildCall2 with exactly the fixed parameter count on a vararg function
   type is legal. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef one = i32;
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, &one, 1, true);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fn_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef args[1] = { LLVMConstInt(i32, 42, false) };
    LLVMBuildCall2(builder, fn_ty, fn, args, 1, "call");
    LLVMBuildUnreachable(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("vararg exact ok\n");
    return 0;
}
