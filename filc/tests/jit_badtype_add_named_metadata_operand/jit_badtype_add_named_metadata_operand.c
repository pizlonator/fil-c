#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMValueRef mdstr = LLVMMDStringInContext(ctx, "s", 1);

    /* LLVMAddNamedMetadataOperand requires a metadata-as-value holding an
       llvm::MDNode or llvm::ConstantAsMetadata; the MDString metadata-as-value
       mdstr is not one.  This must panic in the wrapper instead of tripping an
       LLVM assertion. */
    LLVMAddNamedMetadataOperand(mod, "nmd", mdstr);

    return 0;
}
