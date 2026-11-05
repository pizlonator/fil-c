#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetTargetExtTypeIntParam with an index past the type's parameter
   count.  The accessor indexes its integer-parameter array raw, so an
   out-of-range read returns garbage; the wrapper must panic instead. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    unsigned ints[2] = { 768, 96 };
    LLVMTypeRef ty = LLVMTargetExtTypeInContext(context, "amx.tile",
                                                NULL, 0, ints, 2);
    if (LLVMGetTargetExtTypeNumIntParams(ty) != 2) {
        fprintf(stderr, "bad: int param count\n");
        abort();
    }
    LLVMGetTargetExtTypeIntParam(ty, 9);
    return 0;
}
