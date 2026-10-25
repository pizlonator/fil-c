/* AddGlobalMapping validation: FilLLVMJIT_AddGlobalMapping must reject a
   small integer forged as a pointer.

   A genuine Fil-C capability (here, (void*)&mydata) maps fine. But a small
   integer cast to a pointer is a capability-less (forged) pointer whose
   value cannot be a genuine host code/data address (those never live in the
   first page), so installing it as a JIT mapping must panic instead of
   faulting later when JITed code reaches it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>
#include <filc_llvm_jit.h>


static int mydata = 7;

int main(void)
{
    /* No explicit init: first Create lazily boots the shim (API v4). */
    void *jit = FilLLVMJIT_Create((void *)0);
    if (!jit) {
        printf("create failed\n");
        return 1;
    }
    if (FilLLVMJIT_AddGlobalMapping(jit, (void *)"mydata",
                                           (void *)&mydata)) {
        printf("valid mapping failed\n");
        return 1;
    }
    printf("valid mapping ok\n");
    fflush(stdout);
    void *evil = (void *)1;
    FilLLVMJIT_AddGlobalMapping(jit, (void *)"evil", evil);
    printf("small-int mapping unexpectedly accepted\n");
    return 1;
}
