#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* AggregateRet({i32}, 1) in an i32 function. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef vals[1];
    vals[0] = LLVMConstInt(i32, 1, false);
    LLVMBuildAggregateRet(builder, vals, 1);
    return 0;
}
