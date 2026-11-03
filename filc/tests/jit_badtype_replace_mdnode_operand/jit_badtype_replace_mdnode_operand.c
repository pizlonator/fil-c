#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMValueRef mdstr = LLVMMDStringInContext(ctx, "s", 1);
    LLVMMetadataRef mdmeta = LLVMMDStringInContext2(ctx, "s", 1);

    /* LLVMReplaceMDNodeOperandWith requires a metadata-as-value holding a
       llvm::MDNode; the MDString metadata-as-value mdstr is not one.  This must
       panic in the wrapper instead of tripping an LLVM assertion. */
    LLVMReplaceMDNodeOperandWith(mdstr, 0, mdmeta);

    return 0;
}
