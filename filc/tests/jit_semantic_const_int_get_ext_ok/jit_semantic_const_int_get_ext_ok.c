#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMConstIntGetSExtValue and
   LLVMConstIntGetZExtValue checks: a multi-word (i128) constant whose value
   fits in 64 bits must stay legal, and both getters must still return the
   right values. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i128 = LLVMIntTypeInContext(context, 128);
    unsigned long long words[2] = { 1, 0 };  /* value = 1 */
    LLVMValueRef one = LLVMConstIntOfArbitraryPrecision(i128, 2, words);
    unsigned long long high[2] = { 0x8000000000000000ULL, 0 };
    LLVMValueRef highBit = LLVMConstIntOfArbitraryPrecision(i128, 2, high);

    if (LLVMConstIntGetSExtValue(one) != 1) {
        fprintf(stderr, "bad: wrong sext value\n");
        abort();
    }
    if (LLVMConstIntGetZExtValue(one) != 1) {
        fprintf(stderr, "bad: wrong zext value\n");
        abort();
    }
    if (LLVMConstIntGetZExtValue(highBit) != 0x8000000000000000ULL) {
        fprintf(stderr, "bad: wrong zext high bit value\n");
        abort();
    }

    printf("const int get ext values ok\n");
    return 0;
}
