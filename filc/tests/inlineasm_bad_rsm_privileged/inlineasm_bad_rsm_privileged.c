#include <stdfil.h>

int main(void)
{
    /* RSM (Resume from System Management Mode) is a privileged control-flow
       instruction that returns program control from SMM, restoring processor
       state from the SMRAM dump. It has system-level side effects (restoring
       CR0/CR4, flags, and all processor state) and requires SMM, generating
       #UD otherwise. It is not safe for inline assembly. */
    asm volatile("rsm");
    return 0;
}
