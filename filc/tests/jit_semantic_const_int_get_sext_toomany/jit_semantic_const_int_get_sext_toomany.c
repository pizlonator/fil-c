#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMConstIntGetSExtValue on an i128 whose value needs more than 64
   significant bits.  APInt::getSExtValue asserts "Too many bits for int64_t"
   (llvm/include/llvm/ADT/APInt.h) for it, and the declared bit width says
   nothing about whether the value fits, so the check is a shim predicate
   over the constant's value.  A value that does fit stays legal; see
   jit_semantic_const_int_get_ext_ok. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i128 = LLVMIntTypeInContext(context, 128);
    unsigned long long words[2] = { 0, 1 };  /* value = 2^64 */
    LLVMValueRef v = LLVMConstIntOfArbitraryPrecision(i128, 2, words);

    LLVMConstIntGetSExtValue(v);

    return 0;
}
