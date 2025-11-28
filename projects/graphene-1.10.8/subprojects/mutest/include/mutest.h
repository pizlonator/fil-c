/* µTest
 *
 * Copyright 2019  Emmanuele Bassi
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
# define MUTEST_BEGIN_DECLS     extern "C" {
# define MUTEST_END_DECLS       }
#else
# define MUTEST_BEGIN_DECLS
# define MUTEST_END_DECLS
#endif

#ifndef MUTEST_PUBLIC
# define MUTEST_PUBLIC          extern
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(__GNUC__) && __GNUC__ >= 4
# define MUTEST_NULL_TERMINATED \
  __attribute__((__sentinel__))
#else
# define MUTEST_NULL_TERMINATED
#endif

#ifdef __GNUC__
# define MUTEST_NO_RETURN \
  __attribute__((__noreturn__))
#else
# define MUTEST_NO_RETURN
#endif

#ifdef __GNUC__
# define MUTEST_UNUSED \
  __attribute__((unused))
#else
# define MUTEST_UNUSED
#endif

MUTEST_BEGIN_DECLS

/* {{{ Types */

/**
 * mutest_suite_t:
 *
 * An opaque structure representing a test suite.
 */
typedef struct _mutest_suite_t mutest_suite_t;

/**
 * mutest_spec_t:
 *
 * An opaque structure representing a spec.
 */
typedef struct _mutest_spec_t mutest_spec_t;

/**
 * mutest_expect_t:
 *
 * An opaque structure representing an expectation.
 */
typedef struct _mutest_expect_t mutest_expect_t;

/**
 * mutest_expect_res_t:
 *
 * An opaque structure representing the result of an expectation.
 */
typedef struct _mutest_expect_res_t mutest_expect_res_t;

/**
 * mutest_describe_func_t:
 * @suite: a #mutest_suite_t
 *
 * The prototype of a function to pass to mutest_describe().
 */
typedef void (* mutest_describe_func_t) (mutest_suite_t *suite);

/**
 * mutest_spec_func_t:
 * @spec: a #mutest_spec_t
 *
 * The prototype of a function to pass to mutest_it().
 */
typedef void (* mutest_spec_func_t) (mutest_spec_t *spec);

/**
 * mutest_matcher_func_t:
 * @e: a #mutest_expect_t
 * @check: the #mutest_expect_res_t to check against @e
 *
 * The prototype of a function to pass to mutest_expect().
 *
 * Returns: true if @check matches the value in @e.
 */
typedef bool (* mutest_matcher_func_t) (mutest_expect_t *e,
                                        mutest_expect_res_t *check);

/**
 * mutest_hook_func_t:
 *
 * The prototype of a function to pass to mutest_before(), mutest_before_each(),
 * mutest_after_each(), and mutest_after().
 */
typedef void (* mutest_hook_func_t) (void);

/* }}} */

/* {{{ Value wrappers */

/**
 * mutest_bool_value:
 * @value: a boolean scalar
 *
 * Wraps a boolean value to pass to mutest_expect().
 *
 * Returns: a newly allocated #mutest_expect_res_t
 */
MUTEST_PUBLIC
mutest_expect_res_t *
mutest_bool_value (bool value);

/**
 * mutest_get_bool_value:
 * @res: a #mutest_expect_res_t
 *
 * Retrieves the boolean value in the result wrapper.
 *
 * Returns: a boolean value
 */
MUTEST_PUBLIC
bool
mutest_get_bool_value (const mutest_expect_res_t *res);

/**
 * mutest_int_value:
 * @value: an integer scalar
 *
 * Wraps an integer value to pass to mutest_expect().
 *
 * Returns: a newly allocated #mutest_expect_res_t
 */
MUTEST_PUBLIC
mutest_expect_res_t *
mutest_int_value (int value);

/**
 * mutest_get_int_value:
 * @res: a #mutest_expect_res_t
 *
 * Retrieves the integer value in the result wrapper.
 *
 * Returns: an integer value
 */
MUTEST_PUBLIC
int
mutest_get_int_value (const mutest_expect_res_t *res);

/**
 * mutest_string_value:
 * @value: a C string
 *
 * Wraps a C string to pass to mutest_expect().
 *
 * Returns: a newly allocated #mutest_expect_res_t
 */
MUTEST_PUBLIC
mutest_expect_res_t *
mutest_string_value (const char *value);

/**
 * mutest_get_string_value:
 * @res: a #mutest_expect_res_t
 *
 * Retrieves the string in the result wrapper.
 *
 * Returns: a string
 */
MUTEST_PUBLIC
const char *
mutest_get_string_value (const mutest_expect_res_t *res);

/**
 * mutest_float_value:
 * @value: a floating point scalar
 *
 * Wraps a floating point value to pass to mutest_expect().
 *
 * Returns: a newly allocated #mutest_expect_res_t
 */
MUTEST_PUBLIC
mutest_expect_res_t *
mutest_float_value (double value);

/**
 * mutest_get_float_value:
 * @res: a #mutest_expect_res_t
 *
 * Retrieves the floating pointer value in the result wrapper.
 *
 * Returns: a floating point value
 */
MUTEST_PUBLIC
double
mutest_get_float_value (const mutest_expect_res_t *res);

/**
 * mutest_int_range:
 * @min: the minimum value of a range
 * @max: the maximum value of a range
 *
 * Wraps a range of integers between @min and @max, inclusive,
 * to pass to mutest_expect().
 *
 * Returns: a newly allocated #mutest_expect_res_t
 */
MUTEST_PUBLIC
mutest_expect_res_t *
mutest_int_range (int min,
                  int max);

/**
 * mutest_get_int_range:
 * @res: a #mutest_expect_res_t
 * @min: return location for the minimum value of the range
 * @max: return location for the maximum value of the range
 *
 * Retrieves the range in the result wrapper.
 */
MUTEST_PUBLIC
void
mutest_get_int_range (const mutest_expect_res_t *res,
                      int *min,
                      int *max);

/**
 * mutest_float_range:
 * @min: the minimum value of a range
 * @max: the maximum value of a range
 *
 * Wraps a range of floating point values between @min and @max,
 * inclusive, to pass to mutest_expect().
 *
 * Returns: a newly allocated #mutest_expect_res_t
 */
MUTEST_PUBLIC
mutest_expect_res_t *
mutest_float_range (double min,
                    double max);

/**
 * mutest_get_float_range:
 * @res: a #mutest_expect_res_t
 * @min: return location for the minimum value of the range
 * @max: return location for the maximum value of the range
 *
 * Retrieves the range in the result wrapper.
 */
MUTEST_PUBLIC
void
mutest_get_float_range (const mutest_expect_res_t *res,
                        double *min,
                        double *max);

/**
 * mutest_pointer:
 * @pointer: a generic pointer
 *
 * Wraps a pointer to pass to mutest_expect().
 *
 * Returns: a newly allocated #mutest_expect_res_t
 */
MUTEST_PUBLIC
mutest_expect_res_t *
mutest_pointer (const void *pointer);

/**
 * mutest_get_pointer:
 * @res: a #mutest_expect_res_t
 *
 * Retrieves the pointer value in the result wrapper.
 *
 * Returns: a pointer value
 */
MUTEST_PUBLIC
const void *
mutest_get_pointer (const mutest_expect_res_t *res);

/* }}} */

/* {{{ Matchers */

/**
 * mutest_not:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t to check agains @e
 *
 * Negates the check performed by the next #mutest_expect_func_t.
 *
 * Returns: true if #mutest_expect_func_t returns false,
 *   and vice versa
 */
MUTEST_PUBLIC
bool
mutest_not (mutest_expect_t *e,
            mutest_expect_res_t *check);

/**
 * mutest_skip:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t to check against @e
 *
 * Unconditionally skips the expectation.
 *
 * Returns: always true
 */
MUTEST_PUBLIC
bool
mutest_skip (mutest_expect_t *e,
             mutest_expect_res_t *check);

/**
 * mutest_to_be_true:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Unconditionally checks that the value in @e is true.
 *
 * Returns: true if the value is true
 *
 * See also: mutest_to_be()
 */
MUTEST_PUBLIC
bool
mutest_to_be_true (mutest_expect_t *e,
                   mutest_expect_res_t *check);

/**
 * mutest_to_be_false:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Unconditionally checks that the value in @e is false.
 *
 * Returns: true if the value is false
 *
 * See also: mutest_to_be()
 */
MUTEST_PUBLIC
bool
mutest_to_be_false (mutest_expect_t *e,
                    mutest_expect_res_t *check);

/**
 * mutest_to_be_null:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Unconditionally checks that the value in @e is %NULL.
 *
 * Returns: true if the value is %NULL
 *
 * See also: mutest_to_be()
 */
MUTEST_PUBLIC
bool
mutest_to_be_null (mutest_expect_t *e,
                   mutest_expect_res_t *check);

/**
 * mutest_to_be:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Compares the value in @e with the value in @check, if their
 * types match.
 *
 * Returns: true if the values are equal
 */
MUTEST_PUBLIC
bool
mutest_to_be (mutest_expect_t *e,
              mutest_expect_res_t *check);

/**
 * mutest_to_be_close_to:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the numeric value in @e is within a specified tolerance
 * of the expected value in @check.
 *
 * Returns: true if the values are near each other
 */
MUTEST_PUBLIC
bool
mutest_to_be_close_to (mutest_expect_t *e,
                       mutest_expect_res_t *check);

/**
 * mutest_to_be_nan:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the floating point value in @e is NaN (not a number).
 *
 * Returns: true if the value is NaN
 */
MUTEST_PUBLIC
bool
mutest_to_be_nan (mutest_expect_t *e,
                  mutest_expect_res_t *check);

/**
 * mutest_to_be_positive_infinity:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the floating point value in @e is a positive infinity.
 *
 * Returns: true if the value is the expected positive infinity
 */
MUTEST_PUBLIC
bool
mutest_to_be_positive_infinity (mutest_expect_t *e,
                                mutest_expect_res_t *check);

/**
 * mutest_to_be_negative_infinity:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the floating point value in @e is a negative infinity.
 *
 * Returns: true if the value is the expected negative infinity
 */
MUTEST_PUBLIC
bool
mutest_to_be_negative_infinity (mutest_expect_t *e,
                                mutest_expect_res_t *check);

/**
 * mutest_to_be_greater_than:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the numeric value in @e is greater than the value in @check.
 *
 * Returns: true if the value is greater than the expected value
 */
MUTEST_PUBLIC
bool
mutest_to_be_greater_than (mutest_expect_t *e,
                           mutest_expect_res_t *check);

/**
 * mutest_to_be_greater_than_or_equal:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the numeric value in @e is greater than or equal to
 * the value in @check.
 *
 * Returns: true if the value is greater than or equal to the expected value
 */
MUTEST_PUBLIC
bool
mutest_to_be_greater_than_or_equal (mutest_expect_t *e,
                                    mutest_expect_res_t *check);

/**
 * mutest_to_be_less_than_or_equal:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the numeric value in @e is less than or equal to
 * the value in @check.
 *
 * Returns: true if the value is less than or equal to the expected value
 */
MUTEST_PUBLIC
bool
mutest_to_be_less_than_or_equal (mutest_expect_t *e,
                                 mutest_expect_res_t *check);

/**
 * mutest_to_be_less_than:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the numeric value in @e is less than the value in @check.
 *
 * Returns: true if the value is less than the expected value
 */
MUTEST_PUBLIC
bool
mutest_to_be_less_than (mutest_expect_t *e,
                        mutest_expect_res_t *check);

/**
 * mutest_to_be_in_range:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the numeric value in @e is inside the range in @check.
 *
 * Returns: true if the value is inside the expected range
 */
MUTEST_PUBLIC
bool
mutest_to_be_in_range (mutest_expect_t *e,
                       mutest_expect_res_t *check);

/**
 * mutest_to_contain:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks if the numeric range, or string, in @e contains the numeric
 * value, or string, in @check.
 *
 * Returns: true if the range or string contains the expected value
 */
MUTEST_PUBLIC
bool
mutest_to_contain (mutest_expect_t *e,
                   mutest_expect_res_t *check);

/**
 * mutest_to_contain_string:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks that the value in @e is a string containing the
 * string inside @check.
 *
 * Returns: true if the substring is present
 */
MUTEST_PUBLIC
bool
mutest_to_contain_string (mutest_expect_t *e,
                          mutest_expect_res_t *check);

/**
 * mutest_to_start_with_string:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks that the value inside @e contains a string that
 * starts with the value contained inside @check.
 *
 * Returns: true if the value ends with the expected string
 */
MUTEST_PUBLIC
bool
mutest_to_start_with_string (mutest_expect_t *e,
                             mutest_expect_res_t *check);

/**
 * mutest_to_end_with_string:
 * @e: a #mutest_expect_t
 * @check: a #mutest_expect_res_t
 *
 * Checks that the value inside @e contains a string that
 * ends with the value contained inside @check.
 *
 * Returns: true if the value ends with the expected string
 */
MUTEST_PUBLIC
bool
mutest_to_end_with_string (mutest_expect_t *e,
                           mutest_expect_res_t *check);

/**
 * mutest_expect_value:
 * @expect: a #mutest_expect_t
 *
 * Retrieves the value associated with the expectation.
 *
 * You can use this function to write your custom matchers for
 * mutest_expect(). For instance:
 *
 * ```cpp
 * static bool
 * custom_pointer_match (mutest_expect_t *e,
 *                       mutest_expect_res_t *check)
 * {
 *   mutest_expect_res_t *value = mutest_expect_value (e);
 *
 *   return mutest_get_pointer (value) == mutest_get_pointer (check);
 * }
 * ```
 *
 * Returns: the #mutest_expect_res_t used when constructing the
 *   expectation
 */
MUTEST_PUBLIC
mutest_expect_res_t *
mutest_expect_value (mutest_expect_t *expect);

/* }}} */

/* {{{ Main API */

/**
 * mutest_suite_skip:
 * @reason: the reason why the suite was skipped
 *
 * Skips the current suite.
 *
 * You can call this function from within a #mutest_describe_func_t,
 * or in a #mutest_hook_func_t set with mutest_before().
 */
MUTEST_PUBLIC
void
mutest_suite_skip (const char *reason);

/**
 * mutest_spec_skip:
 * @reason: the reason why the specification was skipped
 *
 * Skips the current specification.
 *
 * You can call this function from within a #mutest_spec_func_t,
 * or in a #mutest_hook_func_t set with mutest_before_each().
 */
MUTEST_PUBLIC
void
mutest_spec_skip (const char *reason);

MUTEST_PUBLIC
void
mutest_expect_full (const char *file,
                    int line,
                    const char *func_name,
                    const char *description,
                    mutest_expect_res_t *value,
                    mutest_matcher_func_t first_matcher_func,
                    ...) MUTEST_NULL_TERMINATED;

/**
 * mutest_expect:
 * @description: the description of an expectation
 * @value: the value of the expectation
 * @expect_func: a function used to check against the @value
 * @...: a %NULL-terminated list of functions that will check @value
 *   against expected values
 *
 * Defines a new expectation.
 *
 * An expectation is satisfied if all the @expect_func validate the
 * given @value.
 */
#define mutest_expect(description,value,expect_func,...) \
  mutest_expect_full (__FILE__, __LINE__, __func__, description, value, expect_func, __VA_ARGS__)

MUTEST_PUBLIC
void
mutest_it_full (const char *file,
                int line,
                const char *func_name,
                const char *description,
                mutest_spec_func_t func);

/**
 * mutest_it:
 * @description: the description of a test specification
 * @func: the function to be called to initialize the specification
 *
 * Describes a new test specification.
 *
 * Specifications group various expectations; typically you will use
 * mutest_expect() to define the various expectations that need to
 * be satisfied in order to pass the tests.
 */
#define mutest_it(description,func) \
  mutest_it_full (__FILE__, __LINE__, __func__, description, \
    (mutest_spec_func_t)(void (*)(void)) func)

MUTEST_PUBLIC
void
mutest_describe_full (const char *file,
                      int line,
                      const char *func_name,
                      const char *description,
                      mutest_describe_func_t func);

/**
 * mutest_describe:
 * @description: the description of a test suite
 * @func: the function to be called to initialize the suite
 *
 * Describes a new test suite.
 *
 * Test suites group various specifications; typically you will use
 * mutest_it() to define the specifications for a suite.
 *
 * Each test binary can contain multiple test suites.
 */
#define mutest_describe(description,func) \
  mutest_describe_full (__FILE__, __LINE__, __func__, description, \
    (mutest_describe_func_t)(void (*)(void)) func)

/* }}} */

/* {{{ Hooks */

/**
 * mutest_before:
 * @hook: a #mutest_hook_func_t function
 *
 * Sets the function to be called once before a suite
 * defined by mutest_describe().
 *
 * For instance:
 *
 * |[<!-- language="C" -->
 * static void
 * before_func (void)
 * {
 * }
 *
 * static void
 * hooks_suite (mutest_suite_t *suite)
 * {
 *   // test cases
 * }
 *
 * MUTEST_MAIN (
 *   // runs once before all tests cases
 *   mutest_before (before_func);
 *
 *   mutest_describe ("hooks", hooks_suite);
 * )
 * ]|
 */
MUTEST_PUBLIC
void
mutest_before (mutest_hook_func_t hook);

/**
 * mutest_after:
 * @hook: a #mutest_hook_func_t function
 *
 * Sets the function to be called once after a suite
 * defined by mutest_describe().
 *
 * For instance:
 *
 * |[<!-- language="C" -->
 * static void
 * after_func (void)
 * {
 * }
 *
 * static void
 * hooks_suite (mutest_suite_t *suite)
 * {
 *   // test cases
 * }
 *
 * MUTEST_MAIN (
 *   // runs once after all tests cases
 *   mutest_after (after_func);
 *
 *   mutest_describe ("hooks", hooks_suite);
 * )
 * ]|
 */
MUTEST_PUBLIC
void
mutest_after (mutest_hook_func_t hook);

/**
 * mutest_before_each:
 * @hook: a #mutest_hook_func_t function
 *
 * Sets the function to be called before every specification
 * defined by mutest_it().
 *
 * For instance:
 *
 * |[<!-- language="C" -->
 * static void
 * before_each_func (void)
 * {
 * }
 *
 * static void
 * hooks_suite (mutest_suite_t *suite)
 * {
 *   // runs before each tests case
 *   mutest_before_each (before_each_func);
 *
 *   // test cases
 * }
 *
 * MUTEST_MAIN (
 *   mutest_describe ("hooks", hooks_suite);
 * )
 * ]|
 */
MUTEST_PUBLIC
void
mutest_before_each (mutest_hook_func_t hook);

/**
 * mutest_after_each:
 * @hook: a #mutest_hook_func_t function
 *
 * Sets the function to be called after each specification
 * defined by mutest_it().
 *
 * For instance:
 *
 * |[<!-- language="C" -->
 * static void
 * after_each_func (void)
 * {
 * }
 *
 * static void
 * hooks_suite (mutest_suite_t *suite)
 * {
 *   // runs after each tests case
 *   mutest_after_each (after_each_func);
 *
 *   // test cases
 * }
 *
 * MUTEST_MAIN (
 *   mutest_describe ("hooks", hooks_suite);
 * )
 * ]|
 */
MUTEST_PUBLIC
void
mutest_after_each (mutest_hook_func_t hook);

/* }}} */

/* {{{ Entry points */

/**
 * mutest_init:
 *
 * Explicitly initializes µTest.
 *
 * It is not necessary to call this function: mutest_describe() will
 * do it for you.
 */
MUTEST_PUBLIC
void
mutest_init (void);

/**
 * mutest_report:
 *
 * Reports the total results of all suites.
 *
 * Returns: an exit code that can be used to quit the test binary
 *   using consistent values
 */
MUTEST_PUBLIC
int
mutest_report (void);

/**
 * MUTEST_MAIN;
 * @_C_: The body of the function
 *
 * A convenience macro that defines the main entry point for a test binary.
 *
 * This function initialises µTest and reports the results.
 */
#define MUTEST_MAIN(_C_) \
int main (int argc MUTEST_UNUSED, \
          char *argv[] MUTEST_UNUSED) { \
  mutest_init (); \
\
  { _C_ } \
\
  return mutest_report (); \
}

/* }}} */

MUTEST_END_DECLS
