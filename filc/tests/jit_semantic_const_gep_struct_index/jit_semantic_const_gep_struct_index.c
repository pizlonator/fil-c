#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstGEP2(struct, global, {0, 42}, 2) on a three-field struct: the
   struct-level index must be a constant below the struct's element count
   (StructType::indexValid, llvm/lib/IR/Type.cpp), or
   ConstantExpr::getGetElementPtr trips the "GEP indices invalid!" assert
   (llvm/lib/IR/Constants.cpp).  Deliberate tripwire: the manifest's
   output-excludes greps for LLVM's assert text. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fieldTys[3] = { i32, LLVMInt64TypeInContext(context), i32 };
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 3, false);
    LLVMValueRef global = LLVMAddGlobal(module, structTy, "g");
    LLVMValueRef indices[2] = { LLVMConstInt(i32, 0, false),
                                LLVMConstInt(i32, 42, false) };
    LLVMConstGEP2(structTy, global, indices, 2);
    return 0;
}
