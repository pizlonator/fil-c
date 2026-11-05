#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetParamAlignment with alignment 0.  Argument::addAttr reaches
   Attribute::getWithAlignment and then Align's constructor, which asserts
   "Value must not be 0" (Alignment.h), so the wrapper's alignment check must
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
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnty);
    LLVMValueRef arg = LLVMGetFirstParam(fn);
    LLVMSetParamAlignment(arg, 0);

    return 0;
}
