#include "common.h"

void test_callback(GCallback *callback, gpointer user_data);

// EXPECT:3: Warning: Test: test_callback: argument callback: Missing (scope) annotation for callback without GDestroyNotify (valid: call, async, forever)
