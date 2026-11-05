#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Both ends of the legal bit-width range work: 1 bit (MIN_INT_BITS) and
   2^23 bits (MAX_INT_BITS). */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef one = LLVMIntTypeInContext(context, 1);
    LLVMTypeRef max = LLVMIntTypeInContext(context, 8388608);
    if (LLVMGetIntTypeWidth(one) != 1 || LLVMGetIntTypeWidth(max) != 8388608) {
        fprintf(stderr, "bad: wrong bit widths\n");
        abort();
    }
    printf("int type ok\n");
    return 0;
}
