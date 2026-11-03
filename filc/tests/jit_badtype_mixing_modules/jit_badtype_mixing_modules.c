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
    LLVMModuleRef modB = LLVMModuleCreateWithNameInContext("testB", ctx);
    LLVMValueRef fnB = LLVMAddFunction(modB, "testB", fnT);
    LLVMBasicBlockRef entryB = LLVMAppendBasicBlockInContext(ctx, fnB, "root");
    LLVMBuilderRef builderB = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(builderB, entryB);

    /* builderB was adopted to modB by a prior position call, and entry belongs
       to mod; the unwrap threads the builder's module and must panic on the
       module mismatch before LLVM mixes modules. */
    LLVMPositionBuilderAtEnd(builderB, entry);

    return 0;
}
