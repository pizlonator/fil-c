#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildLandingPad(builder, ty, personality, 0x40000000): LandingPadInst::
   init reserves NumReservedValues and hands it to User::allocHungoffUses
   (llvm/lib/IR/Instructions.cpp, llvm/lib/IR/User.cpp), which ::operator
   news N * sizeof(Use); a huge count reserves gigabytes of address space and
   dies in std::bad_alloc under a constrained one.  Zero clauses is legal
   (LandingPadInst::init has no count assert in this LLVM), so only the upper
   bound is checked. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildLandingPad(builder, i32, fn, 0x40000000u, "");
    return 0;
}
