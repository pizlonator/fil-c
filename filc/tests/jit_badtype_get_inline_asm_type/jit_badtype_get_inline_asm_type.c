#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);

    /* LLVMGetInlineAsm requires a llvm::FunctionType; the i32 type i32 is not
       one.  This must panic in the wrapper instead of tripping an LLVM
       assertion. */
    LLVMGetInlineAsm(i32, "nop", 3, "", 0, false, false, LLVMInlineAsmDialectATT, false);

    return 0;
}
