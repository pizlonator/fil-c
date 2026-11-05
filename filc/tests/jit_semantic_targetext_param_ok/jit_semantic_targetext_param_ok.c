#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the target-extension-type parameter index checks:
   in-range indices return the parameters the type was built with.  The
   returned handles are fresh wrappers of the same LLVM objects, so the
   comparison is by kind and width, not by handle. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef type_params[1] = { LLVMInt32TypeInContext(context) };
    unsigned int_params[2] = { 768, 96 };
    LLVMTypeRef ty = LLVMTargetExtTypeInContext(context, "test.type",
                                                type_params, 1, int_params, 2);
    LLVMTypeRef got = LLVMGetTargetExtTypeTypeParam(ty, 0);
    if (LLVMGetTypeKind(got) != LLVMIntegerTypeKind ||
        LLVMGetIntTypeWidth(got) != 32) {
        fprintf(stderr, "bad: type param did not come back\n");
        abort();
    }
    if (LLVMGetTargetExtTypeIntParam(ty, 0) != 768 ||
        LLVMGetTargetExtTypeIntParam(ty, 1) != 96) {
        fprintf(stderr, "bad: int params did not come back\n");
        abort();
    }
    printf("targetext param ok\n");
    return 0;
}
