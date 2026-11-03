#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);

    /* LLVMFunctionType with a null param_types array but a nonzero count would
       make LLVM read past a null pointer; the wrapper must panic first. */
    LLVMFunctionType(i32, NULL, 1, false);

    return 0;
}
