#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMValueRef mdstr = LLVMMDStringInContext(ctx, "s", 1);

    /* LLVMGetOperandUse requires a llvm::User; the MDString metadata-as-value
       mdstr is not one.  This must panic in the wrapper instead of tripping an
       LLVM assertion. */
    LLVMGetOperandUse(mdstr, 0);

    return 0;
}
