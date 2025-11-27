#include "common.h"

/**
 * test_invalid_nullable:
 * @param: (nullable):
 * @param2: (nullable):
 *
 * Returns: (nullable):
 */
int test_invalid_nullable(int param, GType param2);

// EXPECT:5: Warning: Test: invalid "nullable" annotation: only valid for pointer types and out parameters
// EXPECT:6: Warning: Test: invalid "nullable" annotation: only valid for pointer types and out parameters
// EXPECT:8: Warning: Test: invalid "nullable" annotation: only valid for pointer types and out parameters
