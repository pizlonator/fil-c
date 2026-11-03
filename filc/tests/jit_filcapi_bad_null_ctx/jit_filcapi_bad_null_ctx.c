#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* A null LLVM context must be rejected by unwrap_context's null check with
   "null LLVM context." (exit 42). */

int main()
{
    LLVMCreateBuilderInContext(NULL);
    return 0;
}
