#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMValueRef notDebugLocOwner = LLVMConstInt(int32Type, 42, false);

    /* LLVMGetDebugLocLine demands a llvm::Instruction, llvm::GlobalVariable,
       or llvm::Function; a ConstantInt is none of them.  This must panic in
       the wrapper instead of tripping an LLVM assertion. */
    unsigned line = LLVMGetDebugLocLine(notDebugLocOwner);

    (void)module;
    (void)line;
    return 0;
}
