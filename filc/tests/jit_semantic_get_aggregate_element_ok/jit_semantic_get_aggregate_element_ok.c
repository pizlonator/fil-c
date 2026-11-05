#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMGetAggregateElement type check: constants of
   struct, array, fixed-vector, and scalable-vector type all stay legal, and
   a non-constant element index past the end still returns null rather than
   panicking (only the constant's TYPE is checked). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fieldTypes[2] = { i32, i32 };
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTypes, 2,
                                                   false);
    LLVMTypeRef arrayTy = LLVMArrayType(i32, 4);
    LLVMTypeRef vecTy = LLVMVectorType(i32, 4);
    LLVMTypeRef svTy = LLVMScalableVectorType(i32, 4);

    LLVMValueRef zeroStruct = LLVMConstNull(structTy);
    LLVMValueRef zeroArray = LLVMConstNull(arrayTy);
    LLVMValueRef zeroVec = LLVMConstNull(vecTy);
    LLVMValueRef zeroSVec = LLVMConstNull(svTy);

    if (!LLVMGetAggregateElement(zeroStruct, 0) ||
        !LLVMGetAggregateElement(zeroArray, 1) ||
        !LLVMGetAggregateElement(zeroVec, 2) ||
        !LLVMGetAggregateElement(zeroSVec, 0)) {
        fprintf(stderr, "bad: aggregate element missing\n");
        abort();
    }
    if (LLVMGetAggregateElement(zeroArray, 99) != NULL) {
        fprintf(stderr, "bad: OOB aggregate element not null\n");
        abort();
    }

    printf("aggregate element ok\n");
    return 0;
}
