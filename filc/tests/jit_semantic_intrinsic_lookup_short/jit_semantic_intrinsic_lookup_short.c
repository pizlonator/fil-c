/* LLVMLookupIntrinsicID with a name shorter than "llvm.": without a check,
   LLVM asserts on starts_with("llvm.") and drop_front(5) underflows in NDEBUG. */
#include <filc_llvm_jit.h>
#include <stdio.h>

int main()
{
    printf("%u\n", LLVMLookupIntrinsicID("llvm", 4));
    return 0;
}
