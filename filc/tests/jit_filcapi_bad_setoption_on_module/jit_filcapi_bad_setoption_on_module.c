#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* FilLLVMJIT_SetOption wants a JIT session; passing an LLVM module must
   panic "expected a JIT session object." (exit 42). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    FilLLVMJIT_SetOption((FilLLVMJITRef)module, "opt-level", "0");
    return 0;
}
