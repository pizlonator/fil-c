#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMIntTypeInContext with 0 bits.  IntegerType::get asserts
   NumBits >= MIN_INT_BITS ("bitwidth too small", Type.cpp); the wrapper's
   bit-width check must reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMIntTypeInContext(context, 0);
    return 0;
}
