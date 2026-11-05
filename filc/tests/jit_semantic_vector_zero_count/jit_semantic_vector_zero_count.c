#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMVectorType with an element count of zero.  FixedVectorType::get asserts
   NumElts > 0 ("#Elements of a VectorType must be greater than 0",
   Type.cpp), and LLVM puts no upper bound on the count.  This must panic in
   the wrapper instead of tripping that assertion.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMVectorType(LLVMInt32TypeInContext(context), 0);
    return 0;
}
