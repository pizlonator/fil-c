#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildInsertValue(builder, agg, value, 42) on a two-field struct:
   InsertValueInst::init stores the index verbatim (llvm/lib/IR/
   Instructions.cpp), but the verifier demands
   ExtractValueInst::getIndexedType(agg type, indices) == the inserted value's
   type ("Invalid InsertValueInst operands!", llvm/lib/IR/Verifier.cpp), so
   the out-of-range index is garbage IR that the JIT would refuse; the same
   check as extractvalue keeps the two symmetric. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fieldTys[2] = { i32, i32 };
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 2, false);
    LLVMTypeRef fnTy = LLVMFunctionType(structTy, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef agg = LLVMBuildAlloca(builder, structTy, "");
    LLVMBuildInsertValue(builder, LLVMBuildLoad2(builder, structTy, agg, ""),
                         LLVMConstInt(i32, 7, false), 42, "");
    return 0;
}
