#include "common.h"

/**
 * test_invalid_allow_none:
 * @param: (allow-none):
 * @param2: (allow-none):
 *
 * Returns: (allow-none):
 */
int test_invalid_allow_none(int param, GType param2);

// EXPECT:5: Warning: Test: invalid "allow-none" annotation: only valid for pointer types and out parameters
// EXPECT:6: Warning: Test: invalid "allow-none" annotation: only valid for pointer types and out parameters
// EXPECT:8: Warning: Test: invalid "allow-none" annotation: only valid for pointer types and out parameters
