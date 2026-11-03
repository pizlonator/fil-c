#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);

    /* LLVMSetUnnamedAddr requires a llvm::GlobalValue; the ConstantInt ci32 is
       not one.  This must panic in the wrapper instead of tripping an LLVM
       assertion. */
    LLVMSetUnnamedAddr(ci32, true);

    return 0;
}
