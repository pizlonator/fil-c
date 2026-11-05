#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMFunctionType with a label return type.  FunctionType's constructor
   asserts isValidReturnType ("invalid return type for function", Type.cpp);
   only function, label, and metadata return types are invalid.  This must
   panic in the wrapper instead of tripping that assertion.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef label = LLVMLabelTypeInContext(context);
    LLVMFunctionType(label, NULL, 0, false);
    return 0;
}
