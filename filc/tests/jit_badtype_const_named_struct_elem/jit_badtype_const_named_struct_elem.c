#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);
    LLVMTypeRef struct2Elems[2] = { i32, i32 };
    LLVMTypeRef struct2 = LLVMStructTypeInContext(ctx, struct2Elems, 2, false);
    LLVMTypeRef fnT = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(mod, "test", fnT);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef lw32 = LLVMBuildLoad2(builder, i32, slot, "v");

    /* LLVMConstNamedStruct requires llvm::Constant elements; the load
       instruction lw32 is not one.  This must panic in the wrapper instead of
       tripping an LLVM assertion. */
    LLVMValueRef elems[1] = { lw32 };
    LLVMConstNamedStruct(struct2, elems, 1);

    return 0;
}
