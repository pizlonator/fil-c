#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNull on a metadata type.  Constant::getNullValue
   llvm_unreachables ("Cannot create a null constant of that type!",
   Constants.cpp) on metadata; the wrapper's type check must reject it
   first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMConstNull(LLVMMetadataTypeInContext(context));
    return 0;
}
