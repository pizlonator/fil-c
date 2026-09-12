/* This test checks if the process correctly handles audit module with BTI
   marking when BTI is enforced.  */

#define AUDIT_MOD       "tst-bti-mod-prot-audit.so"
#define HANDLE_RESTART  TEST_COMPARE (fun (), 42)
#define ALLOW_OUTPUT    sc_allow_none
#define CHECK_OUTPUT

#include "tst-bti-skeleton-audit.c"
