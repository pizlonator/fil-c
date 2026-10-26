/* AddGlobalMapping validation: FilLLVMJIT_AddGlobalMapping must reject a
   forged Fil-C capability.

   A genuine Fil-C capability (here, (void*)&mydata) maps fine. But a
   zmkptr-derived pointer keeps malloc's capability while retargeting the
   address far outside the allocation; installing its raw address as a JIT
   mapping would hand JITed code an unowned address, so the wrapper must
   panic instead.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>
#include <filc_llvm_jit.h>


static int mydata = 7;

int main(void)
{
    /* No explicit init: first Create lazily boots the shim (no init). */
    void *jit = FilLLVMJIT_Create();
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
    void *base = malloc(16);
    void *evil = zmkptr(base, (unsigned long)base + 0x100000);
    FilLLVMJIT_AddGlobalMapping(jit, (void *)"evil", evil);
    printf("forged mapping unexpectedly accepted\n");
    return 1;
}
