#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* A pre-compile LLVMModuleRef is not a JIT executable module, so
   FilLLVMJIT_GetPointerToGlobal must panic (exit 42) rather than hand out a
   pointer into unblessed memory. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    FilLLVMJIT_GetPointerToGlobal((FilLLVMJITModuleRef)module, "test");
    return 0;
}
