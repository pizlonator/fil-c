#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the vector element-count checks: a count of one is
   legal for both fixed and scalable vectors and yields a one-element vector. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef vec = LLVMVectorType(LLVMInt32TypeInContext(context), 1);
    LLVMTypeRef svec = LLVMScalableVectorType(LLVMInt32TypeInContext(context), 1);
    if (LLVMGetTypeKind(vec) != LLVMVectorTypeKind ||
        LLVMGetVectorSize(vec) != 1) {
        fprintf(stderr, "bad: fixed vector of one did not come back\n");
        abort();
    }
    if (LLVMGetTypeKind(svec) != LLVMScalableVectorTypeKind ||
        LLVMGetVectorSize(svec) != 1) {
        fprintf(stderr, "bad: scalable vector of one did not come back\n");
        abort();
    }
    printf("vector one ok\n");
    return 0;
}
