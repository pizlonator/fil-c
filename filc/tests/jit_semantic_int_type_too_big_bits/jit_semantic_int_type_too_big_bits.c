#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMIntTypeInContext with 10,000,000 bits, past MAX_INT_BITS (2^23).
   IntegerType::get asserts NumBits <= MAX_INT_BITS ("bitwidth too large",
   Type.cpp); the wrapper's bit-width check must reject it first. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMIntTypeInContext(context, 10000000);
    return 0;
}
