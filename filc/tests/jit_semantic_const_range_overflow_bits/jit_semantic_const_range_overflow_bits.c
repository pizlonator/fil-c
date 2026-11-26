/* LLVMCreateConstantRangeAttribute with num_bits = UINT_MAX: (num_bits + 63u)
   used to wrap 32-bit unsigned to 0 words and skip bounds checking before LLVM
   computed divideCeil(num_bits, 64). */
#include <filc_llvm_jit.h>
#include <limits.h>
#include <stddef.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMCreateConstantRangeAttribute(context, 98, UINT_MAX, NULL, NULL);
    return 0;
}
