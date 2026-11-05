#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMIntrinsicGetName with an id past the intrinsic table.
   llvm_map_to_intrinsic_id (Core.cpp) asserts "Intrinsic ID out of range",
   so the hand-written wrapper's intrinsic-id check must reject it: this is a
   Fil-C panic (exit 42) instead of that assertion abort.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    size_t length = 0;
    LLVMIntrinsicGetName(9999999, &length);

    return 0;
}
