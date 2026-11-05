#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* AddIncoming(phi-in-test, i32-const, BB-of-other). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidTy = LLVMVoidTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMValueRef other = LLVMAddFunction(module, "other",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef otherEntry = LLVMAppendBasicBlockInContext(context, other, "root");
    LLVMBasicBlockRef next = LLVMAppendBasicBlockInContext(context, fn, "next");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef phi = LLVMBuildPhi(builder, i32, "p");
    LLVMValueRef vals[1];
    LLVMBasicBlockRef blocks[1];
    vals[0] = LLVMConstInt(i32, 1, false);
    blocks[0] = otherEntry;
    LLVMAddIncoming(phi, vals, blocks, 1);
    return 0;
}
