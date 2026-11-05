#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the Const-GEP checks: integer indices (the first one
   of any integer width) must stay legal, a single index is exempt from the
   integer check (any index type goes, including a float), and a fixed-vector
   source element type is supported.  All three wrappers are covered. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef f32 = LLVMFloatTypeInContext(context);
    LLVMTypeRef arrayTy = LLVMArrayType(i32, 8);
    LLVMTypeRef vecTy = LLVMVectorType(i32, 4);
    LLVMTypeRef ptrTy = LLVMPointerTypeInContext(context, 0);
    LLVMValueRef g = LLVMAddGlobal(module, arrayTy, "g");
    LLVMValueRef base = LLVMConstBitCast(g, ptrTy);

    LLVMValueRef two[2] = { LLVMConstInt(i32, 0, false),
                            LLVMConstInt(i64, 1, false) };
    LLVMValueRef oneFloat[1] = { LLVMConstReal(f32, 1.0) };

    if (!LLVMConstGEP2(arrayTy, base, two, 2) ||
        !LLVMConstInBoundsGEP2(arrayTy, base, two, 2) ||
        !LLVMConstGEPWithNoWrapFlags(arrayTy, base, two, 2,
                                     1 /* in bounds */)) {
        fprintf(stderr, "bad: two-index const gep\n");
        abort();
    }
    if (!LLVMConstGEP2(i32, base, oneFloat, 1)) {
        fprintf(stderr, "bad: single-index const gep\n");
        abort();
    }
    if (!LLVMConstGEP2(vecTy, base, two, 2)) {
        fprintf(stderr, "bad: vector const gep\n");
        abort();
    }

    printf("const gep ok\n");
    return 0;
}
