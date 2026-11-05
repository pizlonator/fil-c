#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetTailCallKind with kind 99.  CallInst::setTailCallKind packs the
   kind into a two-bit bitfield and Bitfields.h asserts "value is too big",
   so the wrapper's tail-call-kind check must reject it: this is a Fil-C
   panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", fnty);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef call = LLVMBuildCall2(builder, fnty, callee, NULL, 0, "");
    LLVMSetTailCallKind(call, 99);

    return 0;
}
