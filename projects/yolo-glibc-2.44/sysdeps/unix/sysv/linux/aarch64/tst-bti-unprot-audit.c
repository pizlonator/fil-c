/* This test checks if the process ignores and audit module without BTI
   marking when BTI is enforced.  */

#define AUDIT_MOD       "tst-bti-mod-unprot-audit.so"
#define HANDLE_RESTART
#define ALLOW_OUTPUT    sc_allow_stderr
#define CHECK_OUTPUT \
  TEST_COMPARE_STRING (result.err.buffer, \
		       "ERROR: ld.so: object '" AUDIT_MOD "' " \
		       "cannot be loaded as audit interface: failed to turn " \
		       "on BTI protection; ignored.\n")

#include "tst-bti-skeleton-audit.c"
