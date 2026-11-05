#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildFPCast from f64 to f32 is a legal FPTrunc: both types are
   floating-point, the shapes match, and the scalar sizes differ. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef dbl = LLVMDoubleTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, dbl, "slot");
    LLVMValueRef value = LLVMBuildLoad2(builder, dbl, slot, "v");
    LLVMBuildFPCast(builder, value, flt, "cast");
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("fpcast f64 f32 ok\n");
    return 0;
}
