#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* FilLLVMJIT_BlessPointer wants a JIT session; passing an LLVM module panics
   "expected a JIT session object." (exit 42) before any pointer mapping is
   recorded. */

int main()
{
    static int global;
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    FilLLVMJIT_BlessPointer((FilLLVMJITRef)module, &global);
    return 0;
}
