#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);

    /* LLVMConstInt demands a llvm::IntegerType; the void type is not one.
       This must panic in the wrapper instead of tripping an LLVM assertion. */
    LLVMValueRef bad = LLVMConstInt(LLVMVoidTypeInContext(context), 0, false);

    (void)module;
    (void)bad;
    return 0;
}
