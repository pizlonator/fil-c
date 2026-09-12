/* Checks if the audit module without GCS marking is loaded with default GCS
   support.  */

#define AUDIT_MOD       "tst-gcs-audit1.so"

/* The audit modules should load, to expect the AUDIT function wrapper return
   value.  */
#define HANDLE_RESTART  TEST_COMPARE (fun (), 42)

#define GCS_MODE        "0"

#define ALLOW_OUTPUT    sc_allow_stdout | sc_allow_stderr

#define CHECK_STDOUT \
  TEST_COMPARE_STRING (result.out.buffer, "GCS not enabled\n");

#define CHECK_STDERR

#include "tst-gcs-audit-skeleton.c"
