#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);
    LLVMTypeRef fnT = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(mod, "test", fnT);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");

    /* LLVMBuildGEP2 rejects a null indices element: the wrapper's null-element
       check must panic before LLVM dereferences the array. */
    LLVMValueRef idx[1] = { NULL };
    LLVMBuildGEP2(builder, i32, slot, idx, 1, "gep");

    return 0;
}
