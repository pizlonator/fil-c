#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildStructGEP2(builder, struct, global, 42): IRBuilderBase::
   CreateStructGEP builds a two-index GEP with an i32 constant, and with a
   constant pointer ConstantExpr::getGetElementPtr trips the "GEP indices
   invalid!" assert (llvm/lib/IR/Constants.cpp) for an index outside the
   struct; with a non-constant pointer the instruction is left with a null
   result element type that the verifier rejects.  Deliberate tripwire: the
   manifest's output-excludes greps for LLVM's assert text. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fieldTys[3] = { i32, LLVMInt64TypeInContext(context), i32 };
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 3, false);
    LLVMValueRef global = LLVMAddGlobal(module, structTy, "g");
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildStructGEP2(builder, structTy, global, 42, "");
    return 0;
}
