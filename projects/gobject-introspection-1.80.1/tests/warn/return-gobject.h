#include "common.h"

GObject * test_get_object(void);

// EXPECT:3: Warning: Test: test_get_object: return value: Missing (transfer) annotation

/**
 * test_get_object2:
 *
 */
GObject * test_get_object2(void);

// EXPECT:7: Warning: Test: test_get_object2: return value: Missing (transfer) annotation
