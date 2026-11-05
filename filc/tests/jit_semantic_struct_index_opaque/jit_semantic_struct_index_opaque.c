#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMStructGetTypeAtIndex on an opaque struct.  An opaque struct has no
   contained types, so every index is out of range for
   StructType::getElementType ("Element number out of range!",
   DerivedTypes.h).  This must panic in the wrapper instead of tripping that
   assertion. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef struct_ty = LLVMStructCreateNamed(context, "opaque");
    LLVMStructGetTypeAtIndex(struct_ty, 0);
    return 0;
}
