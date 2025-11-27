#include "common.h"

/**
 * test_invalid_array:
 * @out1: (array foobar):
 **/
void
test_invalid_array (char ***out1);

// EXPECT:5: Warning: Test: invalid "array" annotation option: "foobar"

/**
 * test_invalid_array_zero_terminated:
 * @out1: (array zero-terminated):
 * @out2: (array zero-terminated=foobar):
 **/
void
test_invalid_array_zero_terminated (char ***out1,
                                    char ***out2);

// EXPECT:15: Warning: Test: invalid "array" annotation option "zero-terminated" value "foobar", must be 0 or 1

/**
 * test_invalid_array_fixed_size:
 * @out1: (array fixed-size):
 * @out2: (array fixed-size=foobar):
 **/
void
test_invalid_array_fixed_size (char ***out1,
                               char ***out2);

// EXPECT:25: Warning: Test: "array" annotation option "fixed-size" needs a value
// EXPECT:26: Warning: Test: invalid "array" annotation option "fixed-size" value "foobar", must be an integer

/**
 * test_invalid_array_length:
 * @out1: (array length):
 **/
void
test_invalid_array_length (char ***out1,
                           char ***out2);

// EXPECT:37: Warning: Test: "array" annotation option "length" needs a value
