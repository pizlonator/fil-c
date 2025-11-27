#include "common.h"

/**
 * test_invalid_list_element_type:
 * @l1: (element-type):
 * @l2: (element-type int int):
 */

void test_invalid_list_element_type(GList *l1, GList *l2);

// EXPECT:3: Warning: Test: test_invalid_list_element_type: argument l1: Missing (element-type) annotation
// EXPECT:3: Warning: Test: test_invalid_list_element_type: argument l2: Missing (element-type) annotation
// EXPECT:6: Warning: Test: "element-type" annotation for a list must have exactly one option, not 2 options
// EXPECT:5: Warning: Test: "element-type" annotation takes at least one option, none given
// EXPECT:5: Warning: Test: "element-type" annotation for a list must have exactly one option, not 0 options

/**
 * test_invalid_array_element_type:
 * @a1: (element-type):
 * @a2: (element-type int int):
 */

void test_invalid_array_element_type(const char *a1, const char *a2);

// EXPECT:19: Warning: Test: "element-type" annotation takes at least one option, none given

/**
 * test_invalid_hash_element_type:
 * @h1: (element-type):
 * @h2: (element-type int):
 * @h3: (element-type int int int):
 */

void test_invalid_hash_element_type(GHashTable *h1, GHashTable *h2, GHashTable *h3);

// EXPECT:29: Warning: Test: "element-type" annotation takes at least one option, none given
// EXPECT:29: Warning: Test: "element-type" annotation for a hash table must have exactly two options, not 0 option(s)
// EXPECT:31: Warning: Test: "element-type" annotation takes at most 2 options, 3 given

/**
 * test_invalid_bytearray_element_type:
 * @b1: (element-type):
 * @b2: (element-type int):
 */

void test_invalid_bytearray_element_type(GByteArray *b1, GByteArray *b2);

// EXPECT:42: Warning: Test: "element-type" annotation takes at least one option, none given
// EXPECT:42: Warning: Test: "element-type" annotation for an array must have exactly one option, not 0 options

/**
 * test_invalid_ptrarray_element_type:
 * @p1: (element-type):
 * @p2: (element-type int):
 */

void test_invalid_ptrarray_element_type(GPtrArray *p1, GPtrArray *p2);

// EXPECT:53: Warning: Test: "element-type" annotation takes at least one option, none given
// EXPECT:53: Warning: Test: "element-type" annotation for an array must have exactly one option, not 0 options

/**
 * test_unresolved_element_type:
 *
 * Returns: (element-type Unresolved) (transfer full):
 */

GList* test_unresolved_element_type(void);

/**
 * test_unresolved_value_element_type:
 *
 * Returns: (element-type GLib.Value) (transfer full):
 */

GPtrArray* test_unresolved_value_element_type(void);


// EXPECT:19: Warning: Test: Unknown container Type(target_fundamental=utf8, ctype=char*) for element-type annotation
// EXPECT:20: Warning: Test: Unknown container Type(target_fundamental=utf8, ctype=char*) for element-type annotation
// EXPECT:30: Warning: Test: "element-type" annotation for a hash table must have exactly two options, not 1 option(s)
// EXPECT:31: Warning: Test: "element-type" annotation for a hash table must have exactly two options, not 3 option(s)
// EXPECT:43: Warning: Test: invalid (element-type) for a GByteArray, must be one of guint8, gint8 or gchar
// EXPECT:51: Warning: Test: test_invalid_ptrarray_element_type: argument p1: Missing (element-type) annotation
// EXPECT:54: Warning: Test: invalid (element-type) for a GPtrArray, must be a pointer
// EXPECT:65: Warning: Test: test_unresolved_element_type: Unknown type: 'Unresolved'
// EXPECT:73: Warning: Test: test_unresolved_value_element_type: Unknown type: 'GLib.Value'
