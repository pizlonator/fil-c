/* LLVMLookupIntrinsicID requires a name starting with "llvm."; the runtime
   must refuse other names instead of passing them to LLVM. */
#include <filc_llvm_jit.h>
#include <stdio.h>

int main()
{
    printf("%u\n", LLVMLookupIntrinsicID("memcpy", 6));
    return 0;
}
