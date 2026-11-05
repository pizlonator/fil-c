#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstIntGetZExtValue on an i128 with more than 64 active bits.
   APInt::getZExtValue asserts "Too many bits for uint64_t"
   (llvm/include/llvm/ADT/APInt.h) for it.  See
   jit_semantic_const_int_get_ext_ok for the fitting control. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i128 = LLVMIntTypeInContext(context, 128);
    LLVMValueRef v = LLVMConstAllOnes(i128);

    LLVMConstIntGetZExtValue(v);

    return 0;
}
