#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildSwitch(builder, value, default, 0x40000000): SwitchInst::init
   hands 2 + NumCases * 2 to User::allocHungoffUses (llvm/lib/IR/User.cpp),
   which ::operator news it; 0x40000000 cases reserves ~17GB of operands
   (probed: ~12 seconds and ~17GB of RSS on a large machine, and "terminate
   called after throwing an instance of 'std::bad_alloc'" under a smaller
   address space), and at NumCases >= 0x7FFFFFFF the 2 + NumCases * 2
   computation overflows to zero (llvm/lib/IR/Instructions.cpp), tripping
   SwitchInst::init's own "Value && Default && NumReserved" assert. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildSwitch(builder, LLVMConstInt(i32, 0, false), entry, 0x40000000u);
    return 0;
}
