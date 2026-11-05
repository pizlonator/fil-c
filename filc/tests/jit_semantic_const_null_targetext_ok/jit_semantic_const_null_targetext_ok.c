#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstNull on a target extension type with the zeroinit property is
   legal: getTargetTypeInfo (Type.cpp) gives aarch64.svcount the HasZeroInit
   property, so Constant::getNullValue returns a ConstantTargetNone for it. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMValueRef null_te = LLVMConstNull(LLVMTargetExtTypeInContext(
        context, "aarch64.svcount", NULL, 0, NULL, 0));
    if (!null_te) {
        fprintf(stderr, "bad: no target none\n");
        abort();
    }
    printf("targetext null ok\n");
    return 0;
}
