#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildFPCast from i32 to f32.  IRBuilder::CreateFPCast picks FPTrunc or
   FPExt by comparing scalar sizes and CastInst::Create (Instructions.cpp)
   asserts castIsValid ("Invalid cast!") for the non-floating-point operand,
   so the wrapper's FP-cast check must reject this: this is a Fil-C panic
   (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef flt = LLVMFloatTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);
    LLVMBuildFPCast(builder, ci32, flt, "cast");

    return 0;
}
