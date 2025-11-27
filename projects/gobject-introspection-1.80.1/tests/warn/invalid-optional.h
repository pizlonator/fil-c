#include "common.h"

/**
 * test_invalid_optional:
 * @param: (optional):
 * @param2: (optional):
 * @param3: (optional) (in):
 *
 * Returns: (optional):
 */
int *test_invalid_optional(int param, GObject *param2, int *param3);

// EXPECT:5: Warning: Test: invalid "optional" annotation: only valid for out and inout parameters
// EXPECT:6: Warning: Test: invalid "optional" annotation: only valid for out and inout parameters
// EXPECT:7: Warning: Test: invalid "optional" annotation: only valid for out and inout parameters
// EXPECT:9: Warning: Test: invalid "optional" annotation: only valid for out and inout parameters
