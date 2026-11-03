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
    LLVMBasicBlockRef bb2 = LLVMAppendBasicBlockInContext(ctx, fn, "other");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(builder, entry);

    /* LLVMBuildInvoke2 requires a llvm::FunctionType; the i32 type i32 is not
       one.  This must panic in the wrapper instead of tripping an LLVM
       assertion. */
    LLVMBuildInvoke2(builder, i32, fn, NULL, 0, bb2, bb2, "");

    return 0;
}
