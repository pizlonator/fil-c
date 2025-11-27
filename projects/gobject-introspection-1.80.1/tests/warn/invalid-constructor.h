#include "common.h"

/**
 * test_constructor_invalid_return: (constructor)
 *
 */
int test_constructor_invalid_return();

// EXPECT:7: Warning: Test: test_constructor_invalid_return: Constructors must return an instance of their class

/**
 * test_constructor_invalid_return_pointer: (constructor)
 *
 */
GClosure *test_constructor_invalid_return_pointer();

// EXPECT:15: Warning: Test: test_constructor_invalid_return_pointer: Constructors must belong to the same namespace as the class they belong to

