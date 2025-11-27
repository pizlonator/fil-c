/* See https://bugzilla.gnome.org/show_bug.cgi?id=629708 */

/**
 * test_param_mismatch:
 * @wrong_name: (out):
 *
 */
void test_param_mismatch(int *out);

// EXPECT:5: Warning: Test: test_param_mismatch: unknown parameter 'wrong_name' in documentation comment, should be 'out'

/**
 * test_param_mismatch2:
 * @wrong_name2: (out):
 *
 */
void test_param_mismatch2(int a, int *out2);

// EXPECT:14: Warning: Test: test_param_mismatch2: unknown parameter 'wrong_name2' in documentation comment, should be one of 'a', 'out2'

/**
 * test_param_mismatch3:
 * @a: an integer
 * @wrong_name3: (out):
 *
 */
void test_param_mismatch3(int a, int *out3);

// EXPECT:24: Warning: Test: test_param_mismatch3: unknown parameter 'wrong_name3' in documentation comment, should be 'out3'

/**
 * test_param_missing:
 * @missing: (out):
 *
 */
void test_param_missing(void);

// EXPECT:33: Warning: Test: test_param_missing: unknown parameter 'missing' in documentation comment


/**
 * test_param_vargs:
 * @...: The varargs
 *
 */
void test_param_varargs(int i, ...);

// Should not warn

/**
 * test_undocumentable_param:
 */
void test_undocumentable_param(int);

// EXPECT:53: Warning: Test: symbol='test_undocumentable_param': missing parameter name; undocumentable

void test_undocumentable_param_2(int a, int);

// EXPECT:57: Warning: Test: symbol='test_undocumentable_param_2': missing parameter name; undocumentable

void test_undocumentable_param_3(int, int);

// EXPECT:61: Warning: Test: symbol='test_undocumentable_param_3': missing parameter name; undocumentable
// EXPECT:61: Warning: Test: symbol='test_undocumentable_param_3': missing parameter name; undocumentable

void test_void(void);

// Should not warn
