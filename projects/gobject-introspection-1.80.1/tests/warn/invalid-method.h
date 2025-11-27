#include "common.h"

/**
 * test_method_no_parameters: (method)
 *
 */
void test_method_no_parameters();

// EXPECT:7: Warning: Test: test_method_no_parameters: Methods must have parameters

/**
 * test_method_invalid_first_parameter: (method)
 *
 */
void test_method_invalid_first_parameter(int param);

// EXPECT:15: Warning: Test: test_method_invalid_first_parameter: Methods must have a pointer as their first parameter

/**
 * test_method_invalid_namespace: (method)
 *
 */
void test_method_invalid_namespace(GClosure *param);

// EXPECT:23: Warning: Test: test_method_invalid_namespace: Methods must belong to the same namespace as the class they belong to

