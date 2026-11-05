#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success controls for the intrinsic Tys/overloadedness rules.  The type
   getter only demands Tys for overloaded ids, so a non-overloaded id with Tys
   (of any kind) stays legal, and the declaration getter's exact rule keeps
   every direction that agrees with the id's overloadedness legal: a
   non-overloaded declaration with no Tys, and an overloaded declaration with
   Tys. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned trap_id = LLVMLookupIntrinsicID("llvm.trap", 9);
    unsigned abs_id = LLVMLookupIntrinsicID("llvm.abs", 8);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef label = LLVMLabelTypeInContext(context);
    LLVMTypeRef params[1] = { i64 };
    LLVMTypeRef label_params[1] = { label };
    LLVMValueRef declared;
    if (!trap_id || trap_id == abs_id || !LLVMIntrinsicIsOverloaded(abs_id) ||
        LLVMIntrinsicIsOverloaded(trap_id)) {
        fprintf(stderr, "bad: intrinsic lookup failed\n");
        abort();
    }
    /* GetType: non-overloaded id + Tys is legal (and any Tys kind works). */
    if (LLVMGetTypeKind(LLVMIntrinsicGetType(context, trap_id, params, 1)) !=
        LLVMFunctionTypeKind) {
        fprintf(stderr, "bad: non-overloaded GetType with Tys\n");
        abort();
    }
    if (LLVMGetTypeKind(LLVMIntrinsicGetType(context, trap_id, label_params,
                                             1)) != LLVMFunctionTypeKind) {
        fprintf(stderr, "bad: non-overloaded GetType with label Tys\n");
        abort();
    }
    /* Declaration: non-overloaded id with no Tys is legal. */
    declared = LLVMGetIntrinsicDeclaration(module, trap_id, NULL, 0);
    if (!declared || LLVMGetIntrinsicID(declared) != trap_id) {
        fprintf(stderr, "bad: non-overloaded declaration\n");
        abort();
    }
    /* Declaration: overloaded id with Tys is legal. */
    declared = LLVMGetIntrinsicDeclaration(module, abs_id, params, 1);
    if (!declared || LLVMGetIntrinsicID(declared) != abs_id) {
        fprintf(stderr, "bad: overloaded declaration\n");
        abort();
    }
    printf("intrinsic tys ok\n");
    return 0;
}
