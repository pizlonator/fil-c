#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef flt = LLVMFloatTypeInContext(ctx);

    /* LLVMConstIntOfString requires a llvm::IntegerType; the float type flt is
       not one.  This must panic in the wrapper instead of tripping an LLVM
       assertion. */
    LLVMConstIntOfString(flt, "1", 10);

    return 0;
}
