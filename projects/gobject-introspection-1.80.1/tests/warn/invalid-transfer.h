#include "common.h"

typedef char TestChar;

/**
 * test_transfer_invalid:
 * @param: (transfer):
 * @param2: (transfer invalid):
 * @param3: (transfer full foo):
 * @param4: (transfer full):
 * @param5: (transfer full):
 * @param6: (transfer full):
 * @param7: (transfer container):
 * @param8: (transfer floating):
 */
void test_transfer_invalid(GObject *param, GObject *param2, GObject *param3,
                           char param4, TestChar param5, GType param6,
                           GObject *param7, GDateTime *param8);

// EXPECT:7: Warning: Test: "transfer" annotation needs one option, none given
// EXPECT:8: Warning: Test: invalid "transfer" annotation option: "invalid"
// EXPECT:9: Warning: Test: "transfer" annotation needs one option, 2 given
// EXPECT:10: Warning: Test: invalid "transfer" annotation for gchar: only valid for array, struct, union, boxed, object and interface types
// EXPECT:11: Warning: Test: invalid "transfer" annotation for gchar: only valid for array, struct, union, boxed, object and interface types
// EXPECT:12: Warning: Test: invalid "transfer" annotation for GType: only valid for array, struct, union, boxed, object and interface types
// EXPECT:13: Warning: Test: invalid "transfer" annotation for Class('Object'): only valid for container types
// EXPECT:14: Warning: Test: invalid "transfer" annotation for Record('DateTime'): only valid for object, GVariant and GClosure types

/**
 * test_transfer_return_invalid:
 *
 * Returns: (transfer full):
 */
char test_transfer_return_invalid (void);

// EXPECT:32: Warning: Test: invalid "transfer" annotation for gchar: only valid for array, struct, union, boxed, object and interface types
