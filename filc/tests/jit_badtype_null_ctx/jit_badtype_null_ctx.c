#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    /* A null LLVMContextRef where a context is required must be caught by the
       wrapper's context unwrap (whose null check fires before the generic
       null-object access check would). */
    LLVMInt32TypeInContext(NULL);

    return 0;
}
