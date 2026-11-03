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
    LLVMBuildBr(builder, bb2);
    LLVMPositionBuilderAtEnd(builder, bb2);
    LLVMValueRef phi = LLVMBuildPhi(builder, i32, "phi");
    LLVMBuildBr(builder, entry);

    /* LLVMAddIncoming's incoming_values elements must be value objects; the i32
       type object has the wrong filc object kind and must be rejected during
       the unwrap, before any LLVM code runs. */
    LLVMValueRef vals[1] = { (LLVMValueRef)i32 };
    LLVMBasicBlockRef blocks[1] = { entry };
    LLVMAddIncoming(phi, vals, blocks, 1);

    return 0;
}
