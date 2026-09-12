/* Check if the audit modules without GCS marking is loaded when GCS is
   overridden.  */

#define AUDIT_MOD       "tst-gcs-audit1.so"

/* The audit modules should load, to expect the AUDIT function wrapper return
   value.  */
#define HANDLE_RESTART  TEST_COMPARE (fun (), 42)

#define GCS_MODE        "3"

#define ALLOW_OUTPUT    sc_allow_stdout | sc_allow_stderr

#define CHECK_STDOUT \
  TEST_COMPARE_STRING (result.out.buffer, "GCS enabled\n");

#define CHECK_STDERR

#include "tst-gcs-audit-skeleton.c"
