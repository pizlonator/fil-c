#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMBuildIndirectBr(builder, addr, 0x40000000): IndirectBrInst::init
   reserves 1 + NumDests and hands it to User::allocHungoffUses
   (llvm/lib/IR/Instructions.cpp, llvm/lib/IR/User.cpp), which ::operator
   news N * sizeof(Use); a huge count reserves gigabytes of address space and
   dies in std::bad_alloc under a constrained one. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildIndirectBr(builder,
                        LLVMConstPointerNull(LLVMPointerType(i32, 0)),
                        0x40000000u);
    return 0;
}
