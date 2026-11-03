#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);

    /* LLVMStructGetTypeAtIndex requires a llvm::StructType; the i32 type i32 is
       not one.  This must panic in the wrapper instead of tripping an LLVM
       assertion. */
    LLVMStructGetTypeAtIndex(i32, 0);

    return 0;
}
