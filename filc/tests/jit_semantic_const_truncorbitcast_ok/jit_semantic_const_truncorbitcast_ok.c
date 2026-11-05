#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstTruncOrBitCast(float-const, i32) falls back to bitcast. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMConstTruncOrBitCast(LLVMConstReal(flt, 1.0), i32);
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("const truncorbitcast ok\n");
    return 0;
}