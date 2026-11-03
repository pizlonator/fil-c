#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* A null module pointer to FilLLVMJIT_CompileModule trips the generic Fil-C
   null-object access check inside unwrap_obj (exit 42), before the kind
   check can run. */

int main()
{
    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, NULL);
    return 0;
}
