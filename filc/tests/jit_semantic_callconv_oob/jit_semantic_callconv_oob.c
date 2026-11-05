#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMSetFunctionCallConv with a calling convention that does not fit in
   llvm::CallingConv::MaxID (1023).  Function::setCallingConv asserts
   !(ID & ~CallingConv::MaxID) ("Unsupported calling convention",
   Function.h).  This must panic in the wrapper instead of tripping that
   assertion.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMSetFunctionCallConv(fn, 999999);
    return 0;
}
