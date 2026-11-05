#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMFunctionType with a metadata return type.  FunctionType's constructor
   asserts isValidReturnType ("invalid return type for function", Type.cpp);
   only function, label, and metadata return types are invalid.  This must
   panic in the wrapper instead of tripping that assertion. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef metadata = LLVMMetadataTypeInContext(context);
    LLVMFunctionType(metadata, NULL, 0, false);
    return 0;
}
