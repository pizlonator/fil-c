#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMStructGetTypeAtIndex index check: an in-range
   index on a defined struct returns the field's type. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef fields[2] = { LLVMInt32TypeInContext(context),
                              LLVMInt64TypeInContext(context) };
    LLVMTypeRef struct_ty = LLVMStructTypeInContext(context, fields, 2, false);
    if (LLVMCountStructElementTypes(struct_ty) != 2 ||
        LLVMGetTypeKind(LLVMStructGetTypeAtIndex(struct_ty, 0)) !=
        LLVMIntegerTypeKind ||
        LLVMGetTypeKind(LLVMStructGetTypeAtIndex(struct_ty, 1)) !=
        LLVMIntegerTypeKind) {
        fprintf(stderr, "bad: in-range struct index did not come back\n");
        abort();
    }
    printf("struct index ok\n");
    return 0;
}
