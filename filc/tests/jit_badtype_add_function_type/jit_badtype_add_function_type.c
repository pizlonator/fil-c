#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);

    /* LLVMAddFunction requires a llvm::FunctionType; the i32 type i32 is not
       one.  This must panic in the wrapper instead of tripping an LLVM
       assertion. */
    LLVMAddFunction(mod, "f", i32);

    return 0;
}
