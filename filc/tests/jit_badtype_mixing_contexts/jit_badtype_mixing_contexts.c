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
    LLVMContextRef ctx2 = LLVMContextCreate();
    LLVMTypeRef i32_2 = LLVMInt32TypeInContext(ctx2);
    LLVMTypeRef fnT2 = LLVMFunctionType(i32_2, NULL, 0, false);
    LLVMModuleRef mod2 = LLVMModuleCreateWithNameInContext("test2", ctx2);
    LLVMValueRef fn2 = LLVMAddFunction(mod2, "test2", fnT2);
    LLVMBasicBlockRef entry2 = LLVMAppendBasicBlockInContext(ctx2, fn2, "root");
    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(ctx2);
    LLVMPositionBuilderAtEnd(builder2, entry2);

    /* builder2 belongs to ctx2 while entry belongs to ctx; the wrapper's
       context-consistency unwrap check must panic before LLVM mixes contexts. */
    LLVMPositionBuilderAtEnd(builder2, entry);

    return 0;
}
