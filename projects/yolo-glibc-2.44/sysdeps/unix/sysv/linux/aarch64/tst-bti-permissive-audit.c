/* This test checks that process runs when an LD_AUDIT module
   is not marked with BTI but BTI is not enforced.  */

#define AUDIT_MOD       "tst-bti-mod-unprot-audit.so"
#define HANDLE_RESTART  TEST_COMPARE (fun (), 42)
#define ALLOW_OUTPUT    sc_allow_none
#define CHECK_OUTPUT

#include "tst-bti-skeleton-audit.c"
