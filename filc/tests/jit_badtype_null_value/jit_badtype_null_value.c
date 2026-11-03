#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    /* A null LLVMValueRef reaches the generic Fil-C null-object access check
       inside the wrapper's unwrap; this pins the generic message for value
       arguments. */
    LLVMGetValueName(NULL);

    return 0;
}
