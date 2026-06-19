#include <stdfil.h>

int main(void)
{
    /* "hlt" halts the processor; it is a privileged instruction that must
       never be permitted in safe inline asm. It is not in the allowlist, so
       the validator rejects it as an unsupported mnemonic.

       This canary previously used "orl", but "or" was later added to the
       allowlist (and correctly accepts the b/w/l/q size suffixes), which made
       the test stale. Using a fundamentally-unsafe instruction that will
       never be allowed keeps this canary stable. */
    asm volatile("hlt");
    return 0;
}
