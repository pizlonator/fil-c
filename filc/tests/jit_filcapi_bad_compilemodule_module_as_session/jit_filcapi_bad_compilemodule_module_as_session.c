#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* FilLLVMJIT_CompileModule's session argument must be a JIT session; passing
   an LLVM module panics "expected a JIT session object." (exit 42). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    FilLLVMJIT_CompileModule((FilLLVMJITRef)module, module);
    return 0;
}
