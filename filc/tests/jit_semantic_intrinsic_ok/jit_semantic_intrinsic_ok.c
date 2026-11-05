#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    if (LLVMIntrinsicIsOverloaded(4)) {
        fprintf(stderr, "bad: id 4 should not be overloaded\n");
        abort();
    }
    LLVMTypeRef fn_ty = LLVMIntrinsicGetType(context, 4, NULL, 0);
    if (LLVMGetTypeKind(fn_ty) != LLVMFunctionTypeKind) {
        fprintf(stderr, "bad: intrinsic type is not a function type\n");
        abort();
    }
    LLVMValueRef ifn = LLVMGetIntrinsicDeclaration(module, 4, NULL, 0);
    if (!ifn || LLVMGetIntrinsicID(ifn) != 4) {
        fprintf(stderr, "bad: no intrinsic declaration\n");
        abort();
    }
    /* Keep the declaration alive with a real call. */
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef slot = LLVMBuildAlloca(builder, ptrTy, "slot");
    LLVMValueRef tv = LLVMBuildLoad2(builder, ptrTy, slot, "tv");
    LLVMValueRef args[1];
    args[0] = tv;
    LLVMBuildCall2(builder, LLVMIntrinsicGetType(context, 4, NULL, 0), ifn,
                   args, 1, "adj");
    LLVMBuildRetVoid(builder);
    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }
    printf("intrinsic ok\n");
    return 0;
}
