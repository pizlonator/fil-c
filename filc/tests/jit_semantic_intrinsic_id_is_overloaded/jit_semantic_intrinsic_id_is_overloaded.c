#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMIntrinsicIsOverloaded with an id past Intrinsic::num_intrinsics.
   llvm_map_to_intrinsic_id asserts ID < Intrinsic::num_intrinsics
   ("Intrinsic ID out of range", Core.cpp); the wrapper's intrinsic-id check
   must reject it first. */

int main()
{    LLVMIntrinsicIsOverloaded(9999999);
    return 0;
}
