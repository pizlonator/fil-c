/* LLVMCreateConstantRangeAttribute with narrow empty ranges (including ones
   whose unused top bits differ) and multi-word non-multiple-of-64 ranges. */
#include <filc_llvm_jit.h>
#include <stdint.h>
#include <stdio.h>
#include <stdfil.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    unsigned long long lo8[1] = { 0x100ULL };
    unsigned long long hi8[1] = { 0x200ULL };
    ZASSERT(LLVMCreateConstantRangeAttribute(context, 98, 8, lo8, hi8) != NULL);

    unsigned long long zero[1] = { 0ULL };
    ZASSERT(LLVMCreateConstantRangeAttribute(context, 98, 8, zero, zero) != NULL);

    unsigned long long lo65[2] = { 1ULL, 0ULL };
    unsigned long long hi65[2] = { 2ULL, 0ULL };
    ZASSERT(LLVMCreateConstantRangeAttribute(context, 98, 65, lo65, hi65) != NULL);

    printf("const range narrow ok\n");
    return 0;
}
