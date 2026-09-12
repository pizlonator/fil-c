/* Checks if the audit module without GCS marking is not loaded when GCS is
   enforced.  */

#define AUDIT_MOD       "tst-gcs-audit1.so"

/* The audit moduled should not load, the function returns the expected
   value.  */
#define HANDLE_RESTART  TEST_COMPARE (fun (), 0)

#define GCS_MODE        "1"

#define ALLOW_OUTPUT    sc_allow_stdout | sc_allow_stderr

#define CHECK_STDOUT \
  TEST_COMPARE_STRING (result.out.buffer, \
		       "GCS enabled\n");

#define CHECK_STDERR \
  TEST_COMPARE_STRING (result.err.buffer, \
                      "ERROR: ld.so: object '" AUDIT_MOD "' " \
                      "cannot be loaded as audit interface: " \
                      "not GCS compatible; ignored.\n")

#include "tst-gcs-audit-skeleton.c"
