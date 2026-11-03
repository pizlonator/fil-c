#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);

    /* LLVMFunctionType rejects a null param_types element: the wrapper's
       null-element check must panic before LLVM dereferences the array. */
    LLVMTypeRef elems[1] = { NULL };
    LLVMFunctionType(i32, elems, 1, false);

    return 0;
}
