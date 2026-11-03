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
    LLVMMetadataRef mdmeta = LLVMMDStringInContext2(ctx, "s", 1);

    /* LLVMSetCurrentDebugLocation2 requires a llvm::MDNode; the MDString
       metadata mdmeta is not one.  This must panic in the wrapper instead of
       tripping an LLVM assertion. */
    LLVMSetCurrentDebugLocation2(builder, mdmeta);

    return 0;
}
