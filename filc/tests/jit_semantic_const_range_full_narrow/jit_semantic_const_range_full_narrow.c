/* LLVMCreateConstantRangeAttribute(range, 8 bits, 0xff, 0xff): full narrow
   range whose masked top bits are all ones; must be rejected cleanly before
   Attribute::get asserts !CR.isFullSet(). */
#include <filc_llvm_jit.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    unsigned long long full[1] = { 0xffULL };
    LLVMCreateConstantRangeAttribute(context, 98, 8, full, full);
    return 0;
}
