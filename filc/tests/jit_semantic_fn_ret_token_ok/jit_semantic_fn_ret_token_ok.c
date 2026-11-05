#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMFunctionType return-type check: a token return
   type is not in isValidReturnType's rejected set (function, label, metadata),
   so the type constructor must stay legal and yield a function type. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef token = LLVMTokenTypeInContext(context);
    LLVMTypeRef fn_ty = LLVMFunctionType(token, NULL, 0, false);
    LLVMTypeRef fn_ty2 = LLVMFunctionType(token, &token, 1, true);
    if (LLVMGetTypeKind(fn_ty) != LLVMFunctionTypeKind ||
        LLVMGetTypeKind(fn_ty2) != LLVMFunctionTypeKind) {
        fprintf(stderr, "bad: function type did not come back\n");
        abort();
    }
    printf("fn ret token ok\n");
    return 0;
}
