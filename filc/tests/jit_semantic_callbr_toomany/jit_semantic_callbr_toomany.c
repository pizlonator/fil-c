#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildCallBr with two args on a non-vararg function type taking one
   parameter.  CallBrInst::init (Instructions.cpp) asserts "Calling a function
   with bad signature" for too many args, so the wrapper's argument-count
   check must reject this exactly: this is a Fil-C panic (exit 42) instead of
   that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef one = i32;
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, &one, 1, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fn_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);
    LLVMValueRef args[2] = { ci32, ci32 };
    LLVMBasicBlockRef indirects[1] = { entry };
    LLVMBuildCallBr(builder, fn_ty, fn, entry, indirects, 1, args, 2, NULL,
                    0, "callbr");

    return 0;
}
