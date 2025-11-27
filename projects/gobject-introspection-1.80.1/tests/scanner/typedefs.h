#ifndef __TYPEDEFS_H__
#define __TYPEDEFS_H__

#include <glib-object.h>

#include "gitestmacros.h"

/*
 * Tests for various orderings of typedef struct declarations.
 */


typedef struct {
    int value;
} TypedefsStructWithAnonymousTypedef;


typedef struct _TypedefsStructWithTypedefBefore TypedefsStructWithTypedefBefore;
struct _TypedefsStructWithTypedefBefore {
    int value;
};


/* BUG: produces name with underscore prefix */
struct _TypedefsStructWithTypedefAfter {
    int value;
};
typedef struct _TypedefsStructWithTypedefAfter TypedefsStructWithTypedefAfter;


/* BUG: produces name with underscore prefix */
typedef struct _TypedefsStructWithTagAndTypedef {
    int value;
} TypedefsStructWithTagAndTypedef;


/*
 * Boxed versions of typedef ordering tests.
 */

typedef struct _TypedefsBoxedWithTypedefBefore TypedefsBoxedWithTypedefBefore;
struct _TypedefsBoxedWithTypedefBefore {
    int value;
};

_GI_TEST_EXTERN
GType typedefs_boxed_with_typedef_before_get_type (void) G_GNUC_CONST;


/* BUG: struct has no fields in GIR */
struct _TypedefsBoxedWithTypedefAfter {
    int value;
};
typedef struct _TypedefsBoxedWithTypedefAfter TypedefsBoxedWithTypedefAfter;

_GI_TEST_EXTERN
GType typedefs_boxed_with_typedef_after_get_type (void) G_GNUC_CONST;


/* BUG: struct has no fields and extra <record> with underscore prefix */
typedef struct _TypedefsBoxedWithTagAndTypedef {
    int value;
} TypedefsBoxedWithTagAndTypedef;

_GI_TEST_EXTERN
GType typedefs_boxed_with_tag_and_typedef_get_type (void) G_GNUC_CONST;


/* BUG: extra <record> with underscore prefix */
typedef struct {
    int value;
} TypedefsBoxedWithAnonymousTypedef;

_GI_TEST_EXTERN
GType typedefs_boxed_with_anonymous_typedef_get_type (void) G_GNUC_CONST;


/* BUG: uses <record> instead of <glib:boxed> */
typedef struct _TypedefsBoxedWithHiddenStruct TypedefsBoxedWithHiddenStruct;

_GI_TEST_EXTERN
GType typedefs_boxed_with_hidden_struct_get_type (void) G_GNUC_CONST;


#endif /*__TYPEDEFS_H__*/
