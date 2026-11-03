#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef flt = LLVMFloatTypeInContext(ctx);

    /* LLVMGetIntTypeWidth requires a llvm::IntegerType; the float type flt is
       not one.  This must panic in the wrapper instead of tripping an LLVM
       assertion. */
    LLVMGetIntTypeWidth(flt);

    return 0;
}
