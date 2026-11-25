/* LLVMCreateConstantRangeAttribute(range, 8 bits, 0x105, 0x205): APInt masks
   off bits above bit width, leaving lower == upper == 5, which trips
   ConstantRange's assertion unless the wrapper masks unused top bits too. */
#include <filc_llvm_jit.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    unsigned long long lower[1] = { 0x105ULL };
    unsigned long long upper[1] = { 0x205ULL };
    LLVMCreateConstantRangeAttribute(context, 98, 8, lower, upper);
    return 0;
}
