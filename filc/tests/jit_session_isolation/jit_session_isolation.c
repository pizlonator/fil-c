/* Shim session isolation, proven through a host harness.

   Two shim sessions configure different lazy-resolve callbacks for the SAME
   extern name; each session's JITed getter must resolve through its own
   callback (older code must never call a newer session's callback).  The
   proof itself needs a host process (it dlopens libfilcllvmjit.so and
   drives the FilLLVMJITApi table directly, which a Fil-C program cannot do
   for the shim), so this test shells out to run_host_isolation.sh, which
   builds and runs the harness.  In-process lazy resolution uses the
   Fil-C runtime's process-global resolver state, out of scope for the shim;
   the mapping half of isolation is proven in-process by
   jit_rebind_call's run4/run5.
 */

#include <stdio.h>
#include <stdlib.h>
#include <filc_llvm_jit.h>

int main(void)
{
    int rc =
        system("sh filc/tests/jit_session_isolation/run_host_isolation.sh");
    if (rc) {
        printf("host isolation harness failed: %d\n", rc);
        return 1;
    }
    printf("session isolation hosted OK\n");
    return 0;
}
