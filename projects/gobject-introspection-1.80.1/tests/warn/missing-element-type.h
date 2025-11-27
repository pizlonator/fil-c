#include "common.h"

/**
 * test_gslist_element_type:
 *
 * Returns: (transfer none): Some stuff
 */
GSList *test_gslist_element_type(void);

// EXPECT:6: Warning: Test: test_gslist_element_type: return value: Missing (element-type) annotation

/**
 * test_ptrarray_element_type:
 * @somearray: An array.
 */
void test_ptrarray_arg_element_type(GPtrArray *somearray);

// EXPECT:16: Warning: Test: test_ptrarray_arg_element_type: argument somearray: Missing (element-type) annotation
