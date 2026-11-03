#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);
    LLVMTypeRef fnT = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(mod, "test", fnT);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef lw32 = LLVMBuildLoad2(builder, i32, slot, "v");
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);

    /* LLVMConstXor requires a llvm::Constant; the load instruction lw32 is not
       one.  This must panic in the wrapper instead of tripping an LLVM
       assertion. */
    LLVMConstXor(ci32, lw32);

    return 0;
}
