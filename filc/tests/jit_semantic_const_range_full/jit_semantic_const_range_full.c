/* LLVMCreateConstantRangeAttribute(range, 64 bits, UINT64_MAX, UINT64_MAX):
   Attribute::get asserts !CR.isFullSet() ("ConstantRange attribute must not
   be full", Attributes.cpp); the wrapper must reject full ranges first. */
#include <filc_llvm_jit.h>
#include <stdint.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    unsigned long long full[1] = { UINT64_MAX };
    LLVMCreateConstantRangeAttribute(context, 98, 64, full, full);
    return 0;
}
