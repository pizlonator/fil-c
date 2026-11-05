#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNull on a label type.  Constant::getNullValue llvm_unreachables
   ("Cannot create a null constant of that type!", Constants.cpp) on labels;
   the wrapper's type check must reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMConstNull(LLVMLabelTypeInContext(context));
    return 0;
}
