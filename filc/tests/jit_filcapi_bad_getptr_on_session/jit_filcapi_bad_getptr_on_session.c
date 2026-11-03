#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* FilLLVMJIT_GetPointerToGlobal wants a JIT executable module; passing the
   JIT session must panic "expected a JIT executable module object." (exit
   42) instead of dereferencing the wrong kind of payload. */

int main()
{
    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_GetPointerToGlobal((FilLLVMJITModuleRef)jit, "test");
    return 0;
}
