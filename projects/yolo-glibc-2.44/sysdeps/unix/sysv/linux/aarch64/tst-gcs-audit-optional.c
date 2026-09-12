/* Checks if the audit module without GCS marking is loaded when GCS is
   optional and the GCS is disabled.  */

#define AUDIT_MOD       "tst-gcs-audit1.so"

/* The audit moduled should not load, the function returns the expected
   value.  */
#define HANDLE_RESTART  TEST_COMPARE (fun (), 42)

#define GCS_MODE        "2"

#define ALLOW_OUTPUT    sc_allow_stdout | sc_allow_stderr

#define CHECK_STDOUT \
  TEST_COMPARE_STRING (result.out.buffer, \
		       "GCS not enabled\n");

#define CHECK_STDERR

#include "tst-gcs-audit-skeleton.c"
