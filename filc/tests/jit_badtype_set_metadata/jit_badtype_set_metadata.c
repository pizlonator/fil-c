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
    LLVMValueRef lw32 = LLVMBuildLoad2(builder, i32, slot, "v");
    unsigned kindID = LLVMGetEnumAttributeKindForName("alwaysinline", 12);
    LLVMValueRef mdstr = LLVMMDStringInContext(ctx, "s", 1);

    /* LLVMSetMetadata requires a metadata-as-value holding an llvm::MDNode or
       llvm::ConstantAsMetadata for the metadata operand; the MDString
       metadata-as-value mdstr is not one.  This must panic in the wrapper
       instead of tripping an LLVM assertion. */
    LLVMSetMetadata(lw32, kindID, mdstr);

    return 0;
}
