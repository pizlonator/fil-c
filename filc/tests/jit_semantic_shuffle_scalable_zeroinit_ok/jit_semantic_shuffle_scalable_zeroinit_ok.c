#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMConstShuffleVector over two <vscale x 2 x i32> zeroinitializer
   constants with a scalable zeroinitializer mask is legal:
   ShuffleVectorInst::getShuffleMask reads a scalable zeroinit mask as all
   zeros, and isValidOperands accepts a scalable source whose mask is
   uniform. */

int main()
{    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef svec = LLVMScalableVectorType(i32, 2);
    LLVMValueRef v1 = LLVMConstNull(svec);
    LLVMValueRef v2 = LLVMConstNull(svec);
    LLVMValueRef mask = LLVMConstNull(svec);
    LLVMValueRef s = LLVMConstShuffleVector(v1, v2, mask);
    if (!s) {
        fprintf(stderr, "bad: no shuffle constant\n");
        abort();
    }
    printf("scalable zeroinit shuffle ok\n");
    return 0;
}
