#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildCatchSwitch(builder, parent, unwind, 0x1000001): the handler count
   flows into User::allocHungoffUses (CatchSwitchInst::init,
   llvm/lib/IR/Instructions.cpp), which ::operator news N * sizeof(Use); a
   huge count reserves gigabytes of address space and dies in std::bad_alloc
   under a constrained one.  The count is one past the cap so this fails on
   the new bound, not on the allocation. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildCatchSwitch(builder, fn, entry, 0x1000001u, "");
    return 0;
}
