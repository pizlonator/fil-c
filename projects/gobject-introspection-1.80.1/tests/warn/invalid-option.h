
/**
 * func:
 * @param: (invalid-annotation-option): sizeof (x) == 1
 */
void test_func(int param);

// EXPECT:4: Warning: Test: unknown annotation: invalid-annotation-option

/**
 * func_weird_gtk_doc:
 * @param: Foo (e.g. bar) baz:
 * @param2: This is a (bar):
 */
void test_func_weird_gtk_doc(int param, int param2);
