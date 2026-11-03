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

    /* Belt-and-braces pin of the module-mixing unwrap on the builder position
       path: builderB belongs to modB and entry to mod, so the dedicated
       cannot-move-builder check is shadowed by the unwrap module-mixing check,
       which must fire first and panic. */
    LLVMPositionBuilderAtEnd(builderB, entry);

    return 0;
}
