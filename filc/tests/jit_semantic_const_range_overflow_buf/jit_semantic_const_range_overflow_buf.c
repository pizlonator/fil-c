/* LLVMCreateConstantRangeAttribute with num_bits = UINT_MAX and a 1-word
   buffer: (num_bits + 63u) used to wrap 32-bit unsigned to 0 words and skip
   bounds checking, letting LLVM read 512 MiB past the buffer. */
#include <filc_llvm_jit.h>
#include <limits.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    unsigned long long lower[1] = { 0 };
    unsigned long long upper[1] = { 1 };
    LLVMCreateConstantRangeAttribute(context, 98, UINT_MAX, lower, upper);
    return 0;
}
