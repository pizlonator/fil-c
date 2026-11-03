#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Pin the observed behavior of FilLLVMJIT_GetPointerToGlobal(NULL, "x"):
   unwrap_executable_module returns NULL for a null pointer, but the native
   then locks module->lock on that NULL, so the process dies with SIGSEGV.
   (The plan predicted a graceful NULL return; the crash is what happens
   today.)  There is no stable output fragment for a raw SIGSEGV, so this
   manifest pins only the crash. */

int main()
{
    FilLLVMJIT_GetPointerToGlobal(NULL, "x");
    return 0;
}
