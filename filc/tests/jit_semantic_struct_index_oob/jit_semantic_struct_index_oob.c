#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMStructGetTypeAtIndex with an index past the struct's element count.
   StructType::getElementType asserts N < NumContainedTys ("Element number out
   of range!", DerivedTypes.h).  This must panic in the wrapper instead of
   tripping that assertion.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef fields[2] = { LLVMInt32TypeInContext(context),
                              LLVMInt64TypeInContext(context) };
    LLVMTypeRef struct_ty = LLVMStructTypeInContext(context, fields, 2, false);
    LLVMStructGetTypeAtIndex(struct_ty, 5);
    return 0;
}
