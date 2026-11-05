#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildFPCast from <4 x float> to float.  Both types are
   floating-point, but the shapes differ and the scalar sizes are equal, so
   the builder's FPTrunc/FPExt choice fails castIsValid (Instructions.cpp)
   ("Invalid cast!"), and the wrapper's FP-cast check must reject this: this
   is a Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMTypeRef vec = LLVMVectorType(flt, 4);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, vec, "slot");
    LLVMValueRef v = LLVMBuildLoad2(builder, vec, slot, "v");
    LLVMBuildFPCast(builder, v, flt, "cast");

    return 0;
}
