#include <filc_llvm_jit.h>

#include <stdio.h>

/* LLVMLookupIntrinsicID with a name that does not start with "llvm.".
   LLVM's findTargetSubtable (Intrinsics.cpp) asserts Name.starts_with("llvm.")
   and drops the first five bytes unconditionally, so the wrapper must reject
   this first: this is a Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    printf("%u\n", LLVMLookupIntrinsicID("memcpy", 6));
    return 0;
}
