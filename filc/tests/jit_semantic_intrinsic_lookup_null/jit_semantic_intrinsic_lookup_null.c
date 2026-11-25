/* LLVMLookupIntrinsicID(NULL, 0): passes OPTSTR null-with-zero-length check,
   must be rejected by the prefix check without dereferencing NULL. */
#include <filc_llvm_jit.h>
#include <stddef.h>
#include <stdio.h>

int main()
{
    printf("%u\n", LLVMLookupIntrinsicID(NULL, 0));
    return 0;
}
