#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildAtomicRMW with ordering=Unordered.  AtomicRMWInst::Init asserts
   "atomicrmw instructions cannot be unordered."
   (llvm/lib/IR/Instructions.cpp) for it, so the wrapper's ordering check
   rejects it before LLVM sees it. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMValueRef val = LLVMConstInt(i32, 1, false);
    LLVMBuildAtomicRMW(builder, LLVMAtomicRMWBinOpAdd, slot, val,
                       LLVMAtomicOrderingUnordered, false);

    return 0;
}
