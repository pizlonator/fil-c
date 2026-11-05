#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetInstrParamAlignment with alignment 3.  Attribute::getWithAlignment
   (Attributes.cpp) reaches Align's constructor, which asserts "Alignment is
   not a power of 2" (Alignment.h), so the wrapper's alignment check must
   reject it: this is a Fil-C panic (exit 42) instead of that assertion
   abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnty = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                        &i32, 1, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", fnty);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef arg = LLVMConstInt(i32, 1, false);
    LLVMValueRef call = LLVMBuildCall2(builder, fnty, callee, &arg, 1, "");
    LLVMSetInstrParamAlignment(call, 1, 3);

    return 0;
}
