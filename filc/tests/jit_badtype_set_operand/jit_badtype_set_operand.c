#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);
    LLVMValueRef mdstr = LLVMMDStringInContext(ctx, "s", 1);

    /* LLVMSetOperand requires a llvm::User; the MDString metadata-as-value
       mdstr is not one.  This must panic in the wrapper instead of tripping an
       LLVM assertion. */
    LLVMSetOperand(mdstr, 0, ci32);

    return 0;
}
