#include "common.h"

/**
 * TestInvalidReturnCallback:
 *
 * Returns:
 */
typedef void (*TestInvalidReturnCallback) (void);

// EXPECT:6: Warning: Test: TestInvalidReturnCallback: invalid return annotation

/**
 * test_invalid_return:
 *
 * Returns:
 */
void test_invalid_return (void);

// EXPECT:15: Warning: Test: test_invalid_return: invalid return annotation
