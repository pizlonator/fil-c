#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* A null JIT session must be rejected by unwrap_session's null check with
   "null JIT session." (exit 42). */

int main()
{
    FilLLVMJIT_SetOption(NULL, "opt-level", "0");
    return 0;
}
