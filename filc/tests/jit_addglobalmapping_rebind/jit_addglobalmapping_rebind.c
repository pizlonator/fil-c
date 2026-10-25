/* AddGlobalMapping validation: FilLLVMJIT_AddGlobalMapping must keep
   accepting genuine host addresses, including capability-less (forged) ones.

   dlsym returns a raw host address; round-tripping it through an integer
   forces the capability-less (forged) representation. Remapping such a token
   on another session must succeed: the validation rejects only
   implausible forged pointers (null/small ints) and out-of-bounds
   capabilities, never real host addresses.
 */
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdfil.h>
#include <filc_llvm_jit.h>


static int host_add(int a, int b)
{
    return a + b;
}

int main(void)
{
    /* No explicit init: first Create lazily boots the shim (API v4). */
    void *jit = FilLLVMJIT_Create((void *)0);
    if (!jit) {
        printf("create failed\n");
        return 1;
    }
    if (FilLLVMJIT_AddGlobalMapping(jit, (void *)"host_add",
                                           (void *)host_add)) {
        printf("valid mapping failed\n");
        return 1;
    }
    printf("valid mapping ok\n");
    void *real = dlsym(RTLD_DEFAULT, "printf");
    if (!real) {
        printf("dlsym failed: %s\n", dlerror());
        return 1;
    }
    printf("dlsym ok\n");
    fflush(stdout);
    unsigned long address_bits = (unsigned long)real;
    void *token = (void *)address_bits;
    void *jit2 = FilLLVMJIT_Create((void *)0);
    if (!jit2) {
        printf("create2 failed\n");
        return 1;
    }
    if (FilLLVMJIT_AddGlobalMapping(jit2, (void *)"printf_copy",
                                           token)) {
        printf("rebind failed\n");
        return 1;
    }
    printf("rebind forged token ok\n");
    return 0;
}
