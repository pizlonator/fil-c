#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMValueRef mdstr = LLVMMDStringInContext(ctx, "s", 1);

    /* LLVMGetNumOperands requires a llvm::User or a metadata-as-value holding
       an llvm::MDNode or llvm::ValueAsMetadata; the MDString metadata-as-value
       mdstr is not one.  This must panic in the wrapper instead of tripping an
       LLVM assertion. */
    LLVMGetNumOperands(mdstr);

    return 0;
}
