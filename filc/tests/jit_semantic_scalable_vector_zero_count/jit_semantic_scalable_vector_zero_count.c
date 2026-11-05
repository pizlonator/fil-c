#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMScalableVectorType with a minimum element count of zero.
   ScalableVectorType::get asserts MinNumElts > 0 ("#Elements of a VectorType
   must be greater than 0", Type.cpp), and LLVM puts no upper bound on the
   count.  This must panic in the wrapper instead of tripping that assertion. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMScalableVectorType(LLVMInt32TypeInContext(context), 0);
    return 0;
}
