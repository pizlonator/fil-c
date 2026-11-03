#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* FilLLVMJIT_CompileModule's module argument must be an LLVM module special
   object; passing the JIT session panics "object kind mismatch." (exit 42). */

int main()
{
    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, (LLVMModuleRef)jit);
    return 0;
}
