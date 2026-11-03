#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* FilLLVMJIT_SetGlobalMapping wants a JIT session; passing an LLVM module
   panics "expected a JIT session object." (exit 42). */

int main()
{
    static int global;
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    FilLLVMJIT_SetGlobalMapping((FilLLVMJITRef)module, "k", &global);
    return 0;
}
