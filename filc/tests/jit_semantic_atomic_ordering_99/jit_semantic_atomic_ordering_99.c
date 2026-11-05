#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildAtomicRMW with ordering=99, which is not a valid
   LLVMAtomicOrdering.  The wrapper's ordering check rejects it, so this is a
   Fil-C panic (exit 42) instead of the llvm_unreachable in Core.cpp's
   mapFromLLVMOrdering.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

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
                       (LLVMAtomicOrdering)99, false);

    return 0;
}
