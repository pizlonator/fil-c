#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();

    /* LLVMStructTypeInContext rejects a null element_types element: the
       wrapper's null-element check must panic before LLVM dereferences the
       array. */
    LLVMTypeRef elems[1] = { NULL };
    LLVMStructTypeInContext(ctx, elems, 1, false);

    return 0;
}
