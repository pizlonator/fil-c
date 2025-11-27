#ifndef __GITESTTYPES_H__
#define __GITESTTYPES_H__

#ifndef _GI_DISABLE_CAIRO
#include <cairo-gobject.h>
#endif
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "gitestmacros.h"

_GI_TEST_EXTERN
void regress_set_abort_on_error (gboolean abort_on_error);

/* return annotations */
_GI_TEST_EXTERN
char *regress_test_return_allow_none (void);

_GI_TEST_EXTERN
char *regress_test_return_nullable (void);

/* basic types */
_GI_TEST_EXTERN
gboolean regress_test_boolean (gboolean in);

_GI_TEST_EXTERN
gboolean regress_test_boolean_true (gboolean in);

_GI_TEST_EXTERN
gboolean regress_test_boolean_false (gboolean in);

_GI_TEST_EXTERN
gint8 regress_test_int8 (gint8 in);

_GI_TEST_EXTERN
guint8 regress_test_uint8 (guint8 in);

_GI_TEST_EXTERN
gint16 regress_test_int16 (gint16 in);

_GI_TEST_EXTERN
guint16 regress_test_uint16 (guint16 in);

_GI_TEST_EXTERN
gint32 regress_test_int32 (gint32 in);

_GI_TEST_EXTERN
guint32 regress_test_uint32 (guint32 in);

_GI_TEST_EXTERN
gint64 regress_test_int64 (gint64 in);

_GI_TEST_EXTERN
guint64 regress_test_uint64 (guint64 in);

_GI_TEST_EXTERN
gshort regress_test_short (gshort in);

_GI_TEST_EXTERN
gushort regress_test_ushort (gushort in);

_GI_TEST_EXTERN
gint regress_test_int (gint in);

_GI_TEST_EXTERN
guint regress_test_uint (guint in);

_GI_TEST_EXTERN
glong regress_test_long (glong in);

_GI_TEST_EXTERN
gulong regress_test_ulong (gulong in);

_GI_TEST_EXTERN
gssize regress_test_ssize (gssize in);

_GI_TEST_EXTERN
gsize regress_test_size (gsize in);

_GI_TEST_EXTERN
gfloat regress_test_float (gfloat in);

_GI_TEST_EXTERN
gdouble regress_test_double (gdouble in);

_GI_TEST_EXTERN
gunichar regress_test_unichar (gunichar in);

_GI_TEST_EXTERN
time_t regress_test_timet (time_t in);

_GI_TEST_EXTERN
off_t regress_test_offt (off_t in);

_GI_TEST_EXTERN
GType regress_test_gtype (GType in);

/* utf8 */
_GI_TEST_EXTERN
const char *regress_test_utf8_const_return (void);

_GI_TEST_EXTERN
char *regress_test_utf8_nonconst_return (void);

_GI_TEST_EXTERN
void regress_test_utf8_const_in (const char *in);

_GI_TEST_EXTERN
void regress_test_utf8_out (char **out);

_GI_TEST_EXTERN
void regress_test_utf8_inout (char **inout);

_GI_TEST_EXTERN
GSList *regress_test_filename_return (void);

_GI_TEST_EXTERN
void regress_test_utf8_null_in (char *in);

_GI_TEST_EXTERN
void regress_test_utf8_null_out (char **char_out);

/* in arguments after out arguments */
_GI_TEST_EXTERN
void regress_test_int_out_utf8 (int *length, const char *in);

/* multiple output arguments */

_GI_TEST_EXTERN
void regress_test_multi_double_args (gdouble in, gdouble *one, gdouble *two);

_GI_TEST_EXTERN
void regress_test_utf8_out_out (char **out0, char **out1);

_GI_TEST_EXTERN
char *regress_test_utf8_out_nonconst_return (char **out);

/* non-basic-types */
/* array */
_GI_TEST_EXTERN
gboolean regress_test_strv_in (char **arr);

_GI_TEST_EXTERN
int regress_test_array_int_in (int n_ints, int *ints);

_GI_TEST_EXTERN
void regress_test_array_int_out (int *n_ints, int **ints);

_GI_TEST_EXTERN
void regress_test_array_int_inout (int *n_ints, int **ints);

_GI_TEST_EXTERN
int regress_test_array_gint8_in (int n_ints, gint8 *ints);

_GI_TEST_EXTERN
int regress_test_array_gint16_in (int n_ints, gint16 *ints);

_GI_TEST_EXTERN
gint32 regress_test_array_gint32_in (int n_ints, gint32 *ints);

_GI_TEST_EXTERN
gint64 regress_test_array_gint64_in (int n_ints, gint64 *ints);

_GI_TEST_EXTERN
char *regress_test_array_gtype_in (int n_types, GType *types);

_GI_TEST_EXTERN
const char **regress_test_strv_out_container (void);

_GI_TEST_EXTERN
char **regress_test_strv_out (void);

_GI_TEST_EXTERN
const char * const * regress_test_strv_out_c (void);

_GI_TEST_EXTERN
void   regress_test_strv_outarg (const char ***retp);

_GI_TEST_EXTERN
int regress_test_array_fixed_size_int_in (int *ints);

_GI_TEST_EXTERN
void regress_test_array_fixed_size_int_out (int **ints);

_GI_TEST_EXTERN
int *regress_test_array_fixed_size_int_return (void);

/* transfer tests */
_GI_TEST_EXTERN
int *regress_test_array_int_full_out(int *len);

_GI_TEST_EXTERN
int *regress_test_array_int_none_out(int *len);

_GI_TEST_EXTERN
void regress_test_array_int_null_in (int *arr, int len);

_GI_TEST_EXTERN
void regress_test_array_int_null_out (int **arr, int *len);

/* interface */
/* GList */
_GI_TEST_EXTERN
const GList *regress_test_glist_nothing_return (void);

_GI_TEST_EXTERN
GList *regress_test_glist_nothing_return2 (void);

_GI_TEST_EXTERN
GList *regress_test_glist_container_return (void);

_GI_TEST_EXTERN
GList *regress_test_glist_everything_return (void);

_GI_TEST_EXTERN
void regress_test_glist_gtype_container_in (GList *in);

_GI_TEST_EXTERN
void regress_test_glist_nothing_in (const GList *in);

_GI_TEST_EXTERN
void regress_test_glist_nothing_in2 (GList *in);

_GI_TEST_EXTERN
void regress_test_glist_null_in(GSList *in);

_GI_TEST_EXTERN
void regress_test_glist_null_out(GSList **out_list);

/* GSList */
_GI_TEST_EXTERN
const GSList *regress_test_gslist_nothing_return (void);

_GI_TEST_EXTERN
GSList *regress_test_gslist_nothing_return2 (void);

_GI_TEST_EXTERN
GSList *regress_test_gslist_container_return (void);

_GI_TEST_EXTERN
GSList *regress_test_gslist_everything_return (void);

_GI_TEST_EXTERN
void regress_test_gslist_nothing_in (const GSList *in);

_GI_TEST_EXTERN
void regress_test_gslist_nothing_in2 (GSList *in);

_GI_TEST_EXTERN
void regress_test_gslist_null_in(GSList *in);

_GI_TEST_EXTERN
void regress_test_gslist_null_out(GSList **out_list);

/* GHashTable */
_GI_TEST_EXTERN
const GHashTable *regress_test_ghash_null_return (void);

_GI_TEST_EXTERN
const GHashTable *regress_test_ghash_nothing_return (void);

_GI_TEST_EXTERN
GHashTable *regress_test_ghash_nothing_return2 (void);

_GI_TEST_EXTERN
GHashTable *regress_test_ghash_gvalue_return (void);

_GI_TEST_EXTERN
void regress_test_ghash_gvalue_in (GHashTable *hash);

_GI_TEST_EXTERN
GHashTable *regress_test_ghash_container_return (void);

_GI_TEST_EXTERN
GHashTable *regress_test_ghash_everything_return (void);

_GI_TEST_EXTERN
void regress_test_ghash_null_in (const GHashTable *in);

_GI_TEST_EXTERN
void regress_test_ghash_null_out (const GHashTable **out);

_GI_TEST_EXTERN
void regress_test_ghash_nothing_in (const GHashTable *in);

_GI_TEST_EXTERN
void regress_test_ghash_nothing_in2 (GHashTable *in);

_GI_TEST_EXTERN
GHashTable *regress_test_ghash_nested_everything_return (void);

_GI_TEST_EXTERN
GHashTable *regress_test_ghash_nested_everything_return2 (void);

/* GPtrArray */
_GI_TEST_EXTERN
GPtrArray *regress_test_garray_container_return (void);

_GI_TEST_EXTERN
GPtrArray *regress_test_garray_full_return (void);

/* error? */

/* closure */
_GI_TEST_EXTERN
int regress_test_closure (GClosure *closure);

_GI_TEST_EXTERN
int regress_test_closure_one_arg (GClosure *closure, int arg);

_GI_TEST_EXTERN
GVariant *regress_test_closure_variant (GClosure *closure, GVariant* arg);

/* value */
_GI_TEST_EXTERN
int regress_test_int_value_arg(const GValue *v);

_GI_TEST_EXTERN
const GValue *regress_test_value_return(int i);

/* foreign structs */
#ifndef _GI_DISABLE_CAIRO
_GI_TEST_EXTERN
cairo_t *regress_test_cairo_context_full_return (void);

_GI_TEST_EXTERN
void regress_test_cairo_context_none_in (cairo_t *context);


_GI_TEST_EXTERN
cairo_surface_t *regress_test_cairo_surface_none_return (void);

_GI_TEST_EXTERN
cairo_surface_t *regress_test_cairo_surface_full_return (void);

_GI_TEST_EXTERN
void regress_test_cairo_surface_none_in (cairo_surface_t *surface);

_GI_TEST_EXTERN
void regress_test_cairo_surface_full_out (cairo_surface_t **surface);
#endif

/* versioning (deprecated, since, stability) */
_GI_TEST_EXTERN
void regress_test_versioning (void);


_GI_TEST_EXTERN
GVariant *regress_test_gvariant_i (void);

_GI_TEST_EXTERN
GVariant *regress_test_gvariant_s (void);

_GI_TEST_EXTERN
GVariant *regress_test_gvariant_asv (void);

_GI_TEST_EXTERN
GVariant *regress_test_gvariant_v (void);

_GI_TEST_EXTERN
GVariant *regress_test_gvariant_as (void);

/* enums / flags */

#define NUM_REGRESS_FOO

/**
 * RegressTestEnum:
 * @REGRESS_TEST_VALUE1: value 1
 * @REGRESS_TEST_VALUE2: value 2
 *
 * By purpose, not all members have documentation
 */
typedef enum
{
  REGRESS_TEST_VALUE1,
  REGRESS_TEST_VALUE2,
  REGRESS_TEST_VALUE3 = -1,
  REGRESS_TEST_VALUE4 = '0',
  REGRESS_TEST_VALUE5
} RegressTestEnum;

typedef enum
{
  REGRESS_TEST_UNSIGNED_VALUE1 = 1,
  REGRESS_TEST_UNSIGNED_VALUE2 = 0x80000000
} RegressTestEnumUnsigned;

typedef enum
{
  REGRESS_TEST_FLAG1 = 1 << 0,
  REGRESS_TEST_FLAG2 = 1 << 1,
  REGRESS_TEST_FLAG3 = 1 << 2,
} RegressTestFlags;

_GI_TEST_EXTERN
GType regress_test_enum_get_type (void) G_GNUC_CONST;
#define REGRESS_TEST_TYPE_ENUM (regress_test_enum_get_type ())

_GI_TEST_EXTERN
GType regress_test_enum_unsigned_get_type (void) G_GNUC_CONST;
#define REGRESS_TEST_TYPE_ENUM_UNSIGNED (regress_test_enum_unsigned_get_type ())

_GI_TEST_EXTERN
GType regress_test_flags_get_type (void) G_GNUC_CONST;
#define REGRESS_TEST_TYPE_FLAGS (regress_test_flags_get_type ())

typedef enum
{
  REGRESS_TEST_REFERENCE_0 = 2 + 2,
  REGRESS_TEST_REFERENCE_1 = 1 + 1,
  REGRESS_TEST_REFERENCE_2 = 6 * 9,
  REGRESS_TEST_REFERENCE_3 = REGRESS_TEST_REFERENCE_1 + REGRESS_TEST_REFERENCE_1,
  REGRESS_TEST_REFERENCE_4 = REGRESS_TEST_REFERENCE_2 * REGRESS_TEST_REFERENCE_3,
  REGRESS_TEST_REFERENCE_5 = ~REGRESS_TEST_REFERENCE_4,
} RegressTestReferenceEnum;

/* this is not registered with GType */
typedef enum
{
  REGRESS_TEST_EVALUE1,
  REGRESS_TEST_EVALUE2 = 42,
  REGRESS_TEST_EVALUE3 = '0'
} RegressTestEnumNoGEnum;

/**
 * REGRESS_TEST_EVALUE_DEPRECATED:
 *
 * Scanner used to replace %REGRESS_TEST_EVALUE1 with %REGRESS_TEST_EVALUE_DEPRECATED.
 */
#define REGRESS_TEST_EVALUE_DEPRECATED REGRESS_TEST_EVALUE1

_GI_TEST_EXTERN
const gchar * regress_test_enum_param(RegressTestEnum e);

_GI_TEST_EXTERN
const gchar * regress_test_unsigned_enum_param(RegressTestEnumUnsigned e);


_GI_TEST_EXTERN
void regress_global_get_flags_out (RegressTestFlags *v);

/* error domains */

typedef enum
{
  REGRESS_TEST_ERROR_CODE1 = 1,
  REGRESS_TEST_ERROR_CODE2 = 2,
  REGRESS_TEST_ERROR_CODE3 = 3
} RegressTestError;

_GI_TEST_EXTERN
GType regress_test_error_get_type (void);

_GI_TEST_EXTERN
GQuark regress_test_error_quark (void);

/* Test weird names, with and without
   c_symbol_prefix given by a GType
*/
typedef enum
{
  REGRESS_TEST_ABC_ERROR_CODE1 = 1,
  REGRESS_TEST_ABC_ERROR_CODE2 = 2,
  REGRESS_TEST_ABC_ERROR_CODE3 = 3
} RegressTestABCError;

_GI_TEST_EXTERN
GType regress_test_abc_error_get_type (void);

_GI_TEST_EXTERN
GQuark regress_test_abc_error_quark (void);

typedef enum
{
  REGRESS_TEST_OTHER_ERROR_CODE1 = 1,
  REGRESS_TEST_OTHER_ERROR_CODE2 = 2,
  REGRESS_TEST_OTHER_ERROR_CODE3 = 3
} RegressTestOtherError;

/* This returns a GType for RegressTestOtherError.
   The difference is intentional, although it
   is mainly meant for capitalization problems.
*/
_GI_TEST_EXTERN
GType regress_test_unconventional_error_get_type (void);

_GI_TEST_EXTERN
GQuark regress_test_unconventional_error_quark (void);

typedef enum
{
  REGRESS_TEST_DEF_ERROR_CODE0 = 0,
  REGRESS_TEST_DEF_ERROR_CODE1 = 1,
  REGRESS_TEST_DEF_ERROR_CODE2 = 2
} RegressTestDEFError;

_GI_TEST_EXTERN
GQuark regress_test_def_error_quark (void);

/* the scanner used to have problem
   with two uppercase letter right after
   the identifier prefix, that's why
   we break the RegressTest convention */
typedef enum
{
  REGRESS_ATEST_ERROR_CODE0 = 0,
  REGRESS_ATEST_ERROR_CODE1 = 1,
  REGRESS_ATEST_ERROR_CODE2 = 2
} RegressATestError;

_GI_TEST_EXTERN
GQuark regress_atest_error_quark (void);


/* constants */

#define REGRESS_NEGATIVE_INT_CONSTANT -42
#define REGRESS_INT_CONSTANT 4422
#define REGRESS_DOUBLE_CONSTANT 44.22
#define REGRESS_STRING_CONSTANT "Some String"
#define REGRESS_Mixed_Case_Constant 4423
#define REGRESS_BOOL_CONSTANT TRUE
#define REGRESS_G_GINT64_CONSTANT (G_GINT64_CONSTANT (1000))
#define REGRESS_GUINT64_CONSTANT ((guint64) -1)
#define REGRESS_GOOD_EXPR_CONSTANT (1 + 2 * (3 + 2) << 13 - 4 >> REGRESS_ATEST_ERROR_CODE2)
#define REGRESS_BAD_EXPR_CONSTANT (1 + SOMETHING_UNDEFINED)

typedef guint64 RegressTestTypeGUInt64;
#define REGRESS_GUINT64_CONSTANTA ((RegressTestTypeGUInt64) -1)

/* structures */
typedef struct _RegressTestStructA RegressTestStructA;
typedef struct _RegressTestStructB RegressTestStructB;
typedef struct _RegressTestStructC RegressTestStructC;
typedef struct _RegressTestStructD RegressTestStructD;
typedef struct _RegressTestStructF RegressTestStructF;

struct _RegressTestStructA
{
  gint some_int;
  gint8 some_int8;
  gdouble some_double;
  RegressTestEnum some_enum;
};

_GI_TEST_EXTERN
void regress_test_struct_a_clone (RegressTestStructA *a,
				  RegressTestStructA *a_out);

_GI_TEST_EXTERN
void regress_test_struct_a_parse (RegressTestStructA *a_out, const gchar *string);

_GI_TEST_EXTERN
void regress_test_array_struct_out (RegressTestStructA **arr, int *len);

struct _RegressTestStructB
{
  gint8 some_int8;
  RegressTestStructA nested_a;
};

_GI_TEST_EXTERN
void regress_test_struct_b_clone (RegressTestStructB *b,
			  RegressTestStructB *b_out);

/* This one has a non-basic member */
struct _RegressTestStructC
{
  gint another_int;
  GObject *obj;
};

/* This one has annotated fields */
/**
 * RegressTestStructD:
 * @array1: (array zero-terminated=1):
 * @array2: (array zero-terminated=1) (element-type RegressTestObj):
 * @field: (type RegressTestObj):
 * @list: (element-type RegressTestObj):
 * @garray: (element-type RegressTestObj):
 * @ref_count:
 */
struct _RegressTestStructD
{
  RegressTestStructA **array1;
  gpointer            *array2;
  gpointer             field;
  GList               *list;
  GPtrArray           *garray;
};

/* This one has an array of anonymous unions, inspired by GValue */
struct RegressTestStructE
{
  GType some_type;
  union {
    gint	v_int;
    guint	v_uint;
    glong	v_long;
    gulong	v_ulong;
    gint64      v_int64;
    guint64     v_uint64;
    gfloat	v_float;
    gdouble	v_double;
    gpointer	v_pointer;
  } some_union[2];
};

/* This one has members with const or volatile modifiers. */
struct _RegressTestStructF
{
  volatile gint   ref_count;
  const gint     *data1;
  const gint     *const data2;
  const gint     *const *const data3;
  const gint    **const* data4;
  volatile gint  *const data5;
  const gint     *volatile data6;
  volatile unsigned const char data7;
};

/* plain-old-data boxed types */
typedef struct _RegressTestSimpleBoxedA RegressTestSimpleBoxedA;
typedef struct _RegressTestSimpleBoxedB RegressTestSimpleBoxedB;

/**
 * RegressTestSimpleBoxedA: (copy-func regress_test_simple_boxed_a_copy)
 * @some_int:
 * @some_int8:
 * @some_double:
 * @some_enum:
 *
 * ...
 */
struct _RegressTestSimpleBoxedA
{
  gint some_int;
  gint8 some_int8;
  gdouble some_double;
  RegressTestEnum some_enum;
};

/* Intentionally uses _get_gtype */
_GI_TEST_EXTERN
GType                   regress_test_simple_boxed_a_get_gtype     (void);

_GI_TEST_EXTERN
RegressTestSimpleBoxedA *regress_test_simple_boxed_a_copy         (RegressTestSimpleBoxedA *a);

_GI_TEST_EXTERN
gboolean                regress_test_simple_boxed_a_equals       (RegressTestSimpleBoxedA *a,
                                                          RegressTestSimpleBoxedA *other_a);

_GI_TEST_EXTERN
const RegressTestSimpleBoxedA *regress_test_simple_boxed_a_const_return (void);


struct _RegressTestSimpleBoxedB
{
  gint8 some_int8;
  RegressTestSimpleBoxedA nested_a;
};


_GI_TEST_EXTERN
GType             regress_test_simple_boxed_b_get_type (void);

_GI_TEST_EXTERN
RegressTestSimpleBoxedB *regress_test_simple_boxed_b_copy     (RegressTestSimpleBoxedB *b);

/* opaque boxed */
#define REGRESS_TEST_TYPE_BOXED (regress_test_boxed_get_type())

typedef struct _RegressTestBoxed RegressTestBoxed;
typedef struct _RegressTestBoxedPrivate RegressTestBoxedPrivate;

struct _RegressTestBoxed
{
  gint8 some_int8;
  RegressTestSimpleBoxedA nested_a;

  RegressTestBoxedPrivate *priv;
};

_GI_TEST_EXTERN
GType      regress_test_boxed_get_type (void);

_GI_TEST_EXTERN
RegressTestBoxed *regress_test_boxed_new      (void);

_GI_TEST_EXTERN
RegressTestBoxed *regress_test_boxed_new_alternative_constructor1  (int i);

_GI_TEST_EXTERN
RegressTestBoxed *regress_test_boxed_new_alternative_constructor2  (int i, int j);

_GI_TEST_EXTERN
RegressTestBoxed *regress_test_boxed_new_alternative_constructor3  (char *s);


_GI_TEST_EXTERN
RegressTestBoxed *regress_test_boxed_copy     (RegressTestBoxed *boxed);

_GI_TEST_EXTERN
gboolean   regress_test_boxed_equals   (RegressTestBoxed *boxed,
					RegressTestBoxed *other);


_GI_TEST_EXTERN
void       regress_test_boxeds_not_a_method (RegressTestBoxed *boxed);

_GI_TEST_EXTERN
void       regress_test_boxeds_not_a_static (void);

typedef struct _RegressTestBoxedB RegressTestBoxedB;

struct _RegressTestBoxedB
{
  gint8 some_int8;
  glong some_long;
};


_GI_TEST_EXTERN
GType regress_test_boxed_b_get_type (void);

_GI_TEST_EXTERN
RegressTestBoxedB *regress_test_boxed_b_new (gint8 some_int8, glong some_long);

_GI_TEST_EXTERN
RegressTestBoxedB *regress_test_boxed_b_copy (RegressTestBoxedB *boxed);

typedef struct _RegressTestBoxedC RegressTestBoxedC;

struct _RegressTestBoxedC
{
  guint refcount;
  guint another_thing;
};

_GI_TEST_EXTERN
GType regress_test_boxed_c_get_type (void);

_GI_TEST_EXTERN
RegressTestBoxedC *regress_test_boxed_c_new (void);

/**
 * RegressTestBoxedD: (copy-func regress_test_boxed_d_copy)
 *   (free-func regress_test_boxed_d_free)
 *
 */
typedef struct _RegressTestBoxedD RegressTestBoxedD;


_GI_TEST_EXTERN
GType regress_test_boxed_d_get_type (void);


_GI_TEST_EXTERN
RegressTestBoxedD *regress_test_boxed_d_new (const char *a_string, int a_int);

_GI_TEST_EXTERN
RegressTestBoxedD *regress_test_boxed_d_copy (RegressTestBoxedD *boxed);

_GI_TEST_EXTERN
void regress_test_boxed_d_free (RegressTestBoxedD *boxed);


_GI_TEST_EXTERN
int regress_test_boxed_d_get_magic (RegressTestBoxedD *boxed);

/* gobject */
#define REGRESS_TEST_TYPE_OBJ              (regress_test_obj_get_type ())
#define REGRESS_TEST_OBJECT(object)        (G_TYPE_CHECK_INSTANCE_CAST ((object), REGRESS_TEST_TYPE_OBJ, RegressTestObj))
#define REGRESS_TEST_IS_OBJECT(object)     (G_TYPE_CHECK_INSTANCE_TYPE ((object), REGRESS_TEST_TYPE_OBJ))
#define REGRESS_TEST_OBJ_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), REGRESS_TEST_TYPE_OBJ, RegressTestObjClass))

typedef struct _RegressTestObj          RegressTestObj;
typedef struct _RegressTestObjClass     RegressTestObjClass;

/**
 * RegressTestObj:
 * @bare: (allow-none):
 */
struct _RegressTestObj
{
  GObject parent_instance;

  GObject *bare;
  RegressTestBoxed *boxed;
  GHashTable *hash_table;
  GList *list;
  gint8 some_int8;
  float some_float;
  double some_double;
  char* string;
  GType gtype;
  gint name_conflict;
  GByteArray *byte_array;

  /* < private > */
  void (*function_ptr) (void);
};

typedef void (*RegressTestExternallyDefinedCallback) (RegressTestObj *obj, int someint);

struct _RegressTestObjClass
{
  GObjectClass parent_class;

  int (*matrix) (RegressTestObj *obj, const char *somestr);

  /**
   * RegressTestObjClass::allow_none_vfunc:
   * @two: (allow-none): Another object
   */
  void (*allow_none_vfunc) (RegressTestObj *obj, RegressTestObj *two);

  guint test_signal;
  guint test_signal_with_static_scope_arg;

  RegressTestExternallyDefinedCallback complex_vfunc;

  /* Should be replaced with simple "gpointer" and not be callback */
  void (*_regress_reserved1) (void);
  void (*_regress_reserved2) (void);
};

_GI_TEST_EXTERN
GType      regress_test_obj_get_type (void);

_GI_TEST_EXTERN
RegressTestObj*   regress_test_obj_new (RegressTestObj *obj);

_GI_TEST_EXTERN
RegressTestObj*   regress_constructor (void);

_GI_TEST_EXTERN
RegressTestObj*   regress_test_obj_new_from_file (const char *x, GError **error);

_GI_TEST_EXTERN
void       regress_test_obj_set_bare (RegressTestObj *obj, GObject *bare);

_GI_TEST_EXTERN
void       regress_test_obj_set_string (RegressTestObj *obj, const char *str);

_GI_TEST_EXTERN
const char *regress_test_obj_get_string (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_obj (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_obj_full (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_gstrv_full (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_foreign_struct (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_int64 (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_uint64 (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_array_len_prop (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_inout_int (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_error (RegressTestObj *self);

_GI_TEST_EXTERN
void       regress_test_obj_emit_sig_with_null_error (RegressTestObj *self);

_GI_TEST_EXTERN
int        regress_test_obj_instance_method (RegressTestObj *obj);

_GI_TEST_EXTERN
void       regress_test_obj_instance_method_full (RegressTestObj *obj);

_GI_TEST_EXTERN
double     regress_test_obj_static_method (int x);

_GI_TEST_EXTERN
void       regress_forced_method (RegressTestObj *obj);


_GI_TEST_EXTERN
void regress_test_array_fixed_out_objects (RegressTestObj ***objs);

#if (defined(__GNUC__) || defined(__clang__)) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# define G_GCC_STATIC static
#else
# define G_GCC_STATIC
#endif
_GI_TEST_EXTERN
void regress_test_array_static_in_int (int x[G_GCC_STATIC 10]);

_GI_TEST_EXTERN
void regress_test_obj_torture_signature_0 (RegressTestObj    *obj,
                                   int        x,
                                   double     *y,
                                   int        *z,
                                   const char *foo,
                                   int        *q,
                                   guint       m);

_GI_TEST_EXTERN
gboolean regress_test_obj_torture_signature_1 (RegressTestObj    *obj,
                                       int        x,
                                       double     *y,
                                       int        *z,
                                       const char *foo,
                                       int        *q,
                                       guint       m,
                                       GError    **error);


_GI_TEST_EXTERN
gboolean regress_test_obj_skip_return_val (RegressTestObj *obj,
                                           gint            a,
                                           gint           *out_b,
                                           gdouble         c,
                                           gint           *inout_d,
                                           gint           *out_sum,
                                           gint            num1,
                                           gint            num2,
                                           GError        **error);


_GI_TEST_EXTERN
gboolean regress_test_obj_skip_return_val_no_out (RegressTestObj *obj,
                                                  gint            a,
                                                  GError        **error);


_GI_TEST_EXTERN
gboolean regress_test_obj_skip_param (RegressTestObj *obj,
                                      gint            a,
                                      gint           *out_b,
                                      gdouble         c,
                                      gint           *inout_d,
                                      gint           *out_sum,
                                      gint            num1,
                                      gint            num2,
                                      GError        **error);


_GI_TEST_EXTERN
gboolean regress_test_obj_skip_out_param (RegressTestObj *obj,
                                          gint            a,
                                          gint           *out_b,
                                          gdouble         c,
                                          gint           *inout_d,
                                          gint           *out_sum,
                                          gint            num1,
                                          gint            num2,
                                          GError        **error);


_GI_TEST_EXTERN
gboolean regress_test_obj_skip_inout_param (RegressTestObj *obj,
                                            gint            a,
                                            gint           *out_b,
                                            gdouble         c,
                                            gint           *inout_d,
                                            gint           *out_sum,
                                            gint            num1,
                                            gint            num2,
                                            GError        **error);

/* virtual */
_GI_TEST_EXTERN
int        regress_test_obj_do_matrix (RegressTestObj *obj, const char *somestr);


_GI_TEST_EXTERN
void regress_func_obj_null_in (RegressTestObj *obj);

_GI_TEST_EXTERN
void regress_test_obj_null_out (RegressTestObj **obj);

void regress_func_obj_nullable_in (RegressTestObj *obj);

void regress_test_obj_not_nullable_typed_gpointer_in (RegressTestObj *obj,
                                                      gpointer        input);

void regress_test_obj_not_nullable_element_typed_gpointer_in (RegressTestObj *obj,
                                                              gpointer        input,
                                                              guint           count);

_GI_TEST_EXTERN
void regress_test_obj_name_conflict (RegressTestObj *obj);

/* inheritance */
#define REGRESS_TEST_TYPE_SUB_OBJ           (regress_test_sub_obj_get_type ())
#define REGRESS_TEST_SUB_OBJECT(object)     (G_TYPE_CHECK_INSTANCE_CAST ((object), REGRESS_TEST_TYPE_SUB_OBJ, RegressTestSubObj))
#define REGRESS_TEST_IS_SUB_OBJECT(object)  (G_TYPE_CHECK_INSTANCE_TYPE ((object), REGRESS_TEST_TYPE_SUB_OBJ))
#define REGRESS_TEST_SUB_OBJ_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), REGRESS_TEST_TYPE_SUB_OBJ, RegressTestSubObjClass))

typedef struct _RegressTestSubObj         RegressTestSubObj;
typedef struct _RegressTestSubObjClass    RegressTestSubObjClass;

struct _RegressTestSubObj
{
  RegressTestObj parent_instance;
  /*< private >*/
  gint number;
  gboolean boolean;
};

struct _RegressTestSubObjClass
{
  RegressTestObjClass parent_class;
};


_GI_TEST_EXTERN
GType       regress_test_sub_obj_get_type (void);

_GI_TEST_EXTERN
RegressTestObj* regress_test_sub_obj_new (void);

_GI_TEST_EXTERN
void        regress_test_sub_obj_unset_bare (RegressTestSubObj *obj);

_GI_TEST_EXTERN
int         regress_test_sub_obj_instance_method (RegressTestSubObj *obj);

/* fundamental object */
#define REGRESS_TEST_TYPE_FUNDAMENTAL_OBJECT            (regress_test_fundamental_object_get_type())
#define REGRESS_TEST_IS_FUNDAMENTAL_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REGRESS_TEST_TYPE_FUNDAMENTAL_OBJECT))
#define REGRESS_TEST_IS_FUNDAMENTAL_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), REGRESS_TEST_TYPE_FUNDAMENTAL_OBJECT))
#define REGRESS_TEST_FUNDAMENTAL_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), REGRESS_TEST_TYPE_FUNDAMENTAL_OBJECT, RegressTestFundamentalObjectClass))
#define REGRESS_TEST_FUNDAMENTAL_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REGRESS_TEST_TYPE_FUNDAMENTAL_OBJECT, RegressTestFundamentalObject))
#define REGRESS_TEST_FUNDAMENTAL_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), REGRESS_TEST_TYPE_FUNDAMENTAL_OBJECT, RegressTestFundamentalObjectClass))
#define REGRESS_TEST_FUNDAMENTAL_OBJECT_CAST(obj)       ((RegressTestFundamentalObject*)(obj))

typedef struct _RegressTestFundamentalObject RegressTestFundamentalObject;
typedef struct _RegressTestFundamentalObjectClass RegressTestFundamentalObjectClass;
/**
 * RegressTestFundamentalObjectCopyFunction:
 *
 * Returns: (transfer full):
 */
typedef RegressTestFundamentalObject * (*RegressTestFundamentalObjectCopyFunction) (const RegressTestFundamentalObject *obj);
typedef void (*RegressTestFundamentalObjectFinalizeFunction) (RegressTestFundamentalObject *obj);


struct _RegressTestFundamentalObject {
  GTypeInstance instance;
  gint refcount;
  guint flags;
};

struct _RegressTestFundamentalObjectClass {
  GTypeClass type_class;

  RegressTestFundamentalObjectCopyFunction copy;
  RegressTestFundamentalObjectFinalizeFunction finalize;
};

_GI_TEST_EXTERN
GType 		       regress_test_fundamental_object_get_type (void);

_GI_TEST_EXTERN
RegressTestFundamentalObject* regress_test_fundamental_object_ref      (RegressTestFundamentalObject *fundamental_object);

_GI_TEST_EXTERN
void 		       regress_test_fundamental_object_unref 	(RegressTestFundamentalObject *fundamental_object);

#define REGRESS_TEST_VALUE_HOLDS_FUNDAMENTAL_OBJECT(value)  (G_VALUE_HOLDS(value, REGRESS_TEST_TYPE_FUNDAMENTAL_OBJECT))

_GI_TEST_EXTERN
void 		       regress_test_value_set_fundamental_object (GValue *value, RegressTestFundamentalObject *fundamental_object);

_GI_TEST_EXTERN
RegressTestFundamentalObject* regress_test_value_get_fundamental_object (const GValue *value);

typedef struct _RegressTestFundamentalSubObject RegressTestFundamentalSubObject;
typedef struct _RegressTestFundamentalSubObjectClass RegressTestFundamentalSubObjectClass;

struct _RegressTestFundamentalSubObject
{
  RegressTestFundamentalObject fundamental_object;
  char *data;
};

struct _RegressTestFundamentalSubObjectClass {
  RegressTestFundamentalObjectClass fundamental_object_class;
};


_GI_TEST_EXTERN
GType regress_test_fundamental_sub_object_get_type(void);

struct _RegressTestFundamentalObjectNoGetSetFunc {
  /* This is not really an RegressTestFundamentalObject but we "inherit"
   * the structure from it */
  /* < private > */
  RegressTestFundamentalObject fundamental_object;

  /* < public > */
  char *data;
};

struct _RegressTestFundamentalObjectNoGetSetFuncClass {
  RegressTestFundamentalObjectClass fundamental_object_class;
};

typedef struct _RegressTestFundamentalObjectNoGetSetFunc RegressTestFundamentalObjectNoGetSetFunc;
typedef struct _RegressTestFundamentalObjectNoGetSetFuncClass RegressTestFundamentalObjectNoGetSetFuncClass;

_GI_TEST_EXTERN
GType regress_test_fundamental_object_no_get_set_func_get_type (void);

struct _RegressTestFundamentalSubObjectNoGetSetFunc {
  /* < private > */
  RegressTestFundamentalObjectNoGetSetFunc fundamental_object;
};

struct _RegressTestFundamentalSubObjectNoGetSetFuncClass {
  RegressTestFundamentalObjectNoGetSetFuncClass fundamental_object_class;
};

typedef struct _RegressTestFundamentalSubObjectNoGetSetFunc RegressTestFundamentalSubObjectNoGetSetFunc;
typedef struct _RegressTestFundamentalSubObjectNoGetSetFuncClass RegressTestFundamentalSubObjectNoGetSetFuncClass;

_GI_TEST_EXTERN
GType regress_test_fundamental_sub_object_no_get_set_func_get_type (void);

_GI_TEST_EXTERN
RegressTestFundamentalSubObject *
regress_test_fundamental_sub_object_new (const char *data);

_GI_TEST_EXTERN
RegressTestFundamentalObject *
regress_test_create_fundamental_hidden_class_instance (void);

_GI_TEST_EXTERN
RegressTestFundamentalObjectNoGetSetFunc *
regress_test_fundamental_object_no_get_set_func_new (const char *data);

_GI_TEST_EXTERN
const char *
regress_test_fundamental_object_no_get_set_func_get_data (RegressTestFundamentalObjectNoGetSetFunc *fundamental);

_GI_TEST_EXTERN
RegressTestFundamentalSubObjectNoGetSetFunc *
regress_test_fundamental_sub_object_no_get_set_func_new (const char *data);

_GI_TEST_EXTERN
void
regress_test_fundamental_object_no_get_set_func_make_compatible_with_fundamental_sub_object (void);

/* callback */
typedef void (*RegressTestSimpleCallback) (void);
typedef void RegressTestNoPtrCallback (void);
typedef int (*RegressTestCallback) (void);
typedef int (*RegressTestCallbackUserData) (gpointer user_data);
/**
 * RegressTestCallbackHashtable:
 * @data: (element-type utf8 gint): a hash table; will be modified
 */
typedef void (*RegressTestCallbackHashtable) (GHashTable *data);
typedef void (*RegressTestCallbackGError) (const GError *error);
/**
 * RegressTestCallbackOwnedGError:
 * @error: (transfer full): GError instance; must be freed by the callback
 */
typedef void (*RegressTestCallbackOwnedGError) (GError *error);
/**
 * RegressTestCallbackFull:
 * @foo: the investment rate
 * @bar: how much money
 * @path: (type filename): Path to file
 */
typedef int (*RegressTestCallbackFull) (int foo, double bar, char *path);
/**
 * RegressTestCallbackReturnFull:
 * Return value: (transfer full):
 */
typedef RegressTestObj *(*RegressTestCallbackReturnFull) (void);
/**
 * RegressTestCallbackArray:
 * @one: (array length=one_length):
 * @one_length:
 * @two: (array length=two_length) (element-type utf8):
 * @two_length:
 */
typedef int (*RegressTestCallbackArray) (int *one, gsize one_length, const char** two, int two_length);
/**
 * RegressTestCallbackArrayInOut:
 * @ints: (inout) (array length=length):
 * @length: (inout):
 */
typedef void (* RegressTestCallbackArrayInOut) (int **ints, int *length);

_GI_TEST_EXTERN
void regress_test_simple_callback (RegressTestSimpleCallback callback);

_GI_TEST_EXTERN
void regress_test_noptr_callback (RegressTestNoPtrCallback callback);

_GI_TEST_EXTERN
int regress_test_callback (RegressTestCallback callback);

_GI_TEST_EXTERN
int regress_test_multi_callback (RegressTestCallback callback);

_GI_TEST_EXTERN
int regress_test_array_callback (RegressTestCallbackArray callback);

_GI_TEST_EXTERN
int regress_test_array_inout_callback (RegressTestCallbackArrayInOut callback);

_GI_TEST_EXTERN
int regress_test_callback_user_data (RegressTestCallbackUserData callback,
                             gpointer user_data);

_GI_TEST_EXTERN
void regress_test_callback_return_full (RegressTestCallbackReturnFull callback);

_GI_TEST_EXTERN
int regress_test_callback_destroy_notify (RegressTestCallbackUserData callback,
                                  gpointer user_data,
                                  GDestroyNotify notify);

_GI_TEST_EXTERN
int regress_test_callback_destroy_notify_no_user_data (RegressTestCallbackUserData callback,
                                  GDestroyNotify notify);

_GI_TEST_EXTERN
int regress_test_callback_thaw_notifications (void);


_GI_TEST_EXTERN
void regress_test_callback_async (RegressTestCallbackUserData callback, gpointer user_data);

_GI_TEST_EXTERN
int regress_test_callback_thaw_async (void);


_GI_TEST_EXTERN
void regress_test_async_ready_callback (GAsyncReadyCallback callback);

_GI_TEST_EXTERN
void regress_test_function_async (int                 io_priority,
                                  GCancellable       *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer            user_data);

_GI_TEST_EXTERN
gboolean regress_test_function_finish (GAsyncResult *res, GError **error);

_GI_TEST_EXTERN
gboolean regress_test_function_sync (int io_priority);

_GI_TEST_EXTERN
void regress_test_obj_instance_method_callback (RegressTestObj *obj, RegressTestCallback callback);

_GI_TEST_EXTERN
void regress_test_obj_static_method_callback (RegressTestCallback callback);

_GI_TEST_EXTERN
RegressTestObj *regress_test_obj_new_callback (RegressTestCallbackUserData callback,
                                gpointer user_data,
                                GDestroyNotify notify);

_GI_TEST_EXTERN
void regress_test_obj_new_async (const char          *x G_GNUC_UNUSED,
                                 GCancellable        *cancellable G_GNUC_UNUSED,
                                 GAsyncReadyCallback  callback G_GNUC_UNUSED,
                                 gpointer             user_data G_GNUC_UNUSED);
_GI_TEST_EXTERN
RegressTestObj *regress_test_obj_new_finish (GAsyncResult  *res G_GNUC_UNUSED,
                                             GError       **error G_GNUC_UNUSED);

_GI_TEST_EXTERN
void regress_test_hash_table_callback (GHashTable *data, RegressTestCallbackHashtable callback);

_GI_TEST_EXTERN
void regress_test_gerror_callback (RegressTestCallbackGError callback);

_GI_TEST_EXTERN
void regress_test_null_gerror_callback (RegressTestCallbackGError callback);

_GI_TEST_EXTERN
void regress_test_owned_gerror_callback (RegressTestCallbackOwnedGError callback);


_GI_TEST_EXTERN
void regress_test_skip_unannotated_callback (RegressTestCallback callback);

typedef struct _RegressTestInterface RegressTestInterface;

/* interface */
#define REGRESS_TEST_TYPE_INTERFACE              (regress_test_interface_get_type ())
#define REGRESS_TEST_INTERFACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), REGRESS_TEST_TYPE_INTERFACE, RegressTestInterface))
#define REGRESS_TEST_IS_INTERFACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), REGRESS_TEST_TYPE_INTERFACE))
#define REGRESS_TEST_INTERFACE_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), REGRESS_TEST_TYPE_INTERFACE, RegressTestInterfaceIface))

typedef struct _RegressTestInterfaceIface RegressTestInterfaceIface;

struct _RegressTestInterfaceIface {
    GTypeInterface base_iface;
};

_GI_TEST_EXTERN
GType regress_test_interface_get_type (void) G_GNUC_CONST;

_GI_TEST_EXTERN
void regress_test_interface_emit_signal (RegressTestInterface *self);

/* gobject with non-standard prefix */
#define REGRESS_TEST_TYPE_WI_802_1X              (regress_test_wi_802_1x_get_type ())
#define REGRESS_TEST_WI_802_1X(object)        (G_TYPE_CHECK_INSTANCE_CAST ((object), REGRESS_TEST_TYPE_WI_802_1X, RegressTestWi8021x))
#define REGRESS_TEST_IS_WI_802_1X(object)     (G_TYPE_CHECK_INSTANCE_TYPE ((object), REGRESS_TEST_TYPE_WI_802_1X))
#define REGRESS_TEST_WI_802_1X_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), REGRESS_TEST_TYPE_WI_802_1X, RegressTestWi8021xClass))

typedef struct
{
  GObject parent_instance;

  gboolean testbool;
} RegressTestWi8021x;

typedef struct
{
  GObjectClass parent_class;
} RegressTestWi8021xClass;


_GI_TEST_EXTERN
GType        regress_test_wi_802_1x_get_type (void);

_GI_TEST_EXTERN
RegressTestWi8021x* regress_test_wi_802_1x_new (void);

_GI_TEST_EXTERN
gboolean     regress_test_wi_802_1x_get_testbool (RegressTestWi8021x *obj);

_GI_TEST_EXTERN
void         regress_test_wi_802_1x_set_testbool (RegressTestWi8021x *obj, gboolean v);

_GI_TEST_EXTERN
int          regress_test_wi_802_1x_static_method (int x);

/* floating gobject */
#define REGRESS_TEST_TYPE_FLOATING           (regress_test_floating_get_type ())
#define REGRESS_TEST_FLOATING(object)        (G_TYPE_CHECK_INSTANCE_CAST ((object), REGRESS_TEST_TYPE_FLOATING, RegressTestFloating))
#define REGRESS_TEST_IS_FLOATING(object)     (G_TYPE_CHECK_INSTANCE_TYPE ((object), REGRESS_TEST_TYPE_FLOATING))
#define REGRESS_TEST_FLOATING_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), REGRESS_TEST_TYPE_FLOATING, RegressTestFloatingClass))

typedef struct
{
  GInitiallyUnowned parent_instance;
} RegressTestFloating;

typedef struct
{
  GInitiallyUnownedClass parent_class;
} RegressTestFloatingClass;


_GI_TEST_EXTERN
GType        regress_test_floating_get_type (void);

_GI_TEST_EXTERN
RegressTestFloating* regress_test_floating_new (void);

/* Function signature torture tests */

_GI_TEST_EXTERN
void regress_test_torture_signature_0 (int        x,
                               double     *y,
                               int        *z,
                               const char *foo,
                               int        *q,
                               guint       m);

_GI_TEST_EXTERN
gboolean regress_test_torture_signature_1 (int        x,
                                   double     *y,
                                   int        *z,
                                   const char *foo,
                                   int        *q,
                                   guint       m,
                                   GError    **error);

_GI_TEST_EXTERN
void regress_test_torture_signature_2 (int                  x,
				       RegressTestCallbackUserData  callback,
				       gpointer              user_data,
				       GDestroyNotify        notify,
				       double               *y,
				       int                  *z,
				       const char           *foo,
				       int                  *q,
				       guint                 m);


_GI_TEST_EXTERN
GValue *regress_test_date_in_gvalue (void);

_GI_TEST_EXTERN
GValue *regress_test_strv_in_gvalue (void);

_GI_TEST_EXTERN
GValue *regress_test_null_strv_in_gvalue (void);


_GI_TEST_EXTERN
GObject * _regress_this_is_a_private_symbol (void);


_GI_TEST_EXTERN
void regress_test_multiline_doc_comments (void);

_GI_TEST_EXTERN
void regress_test_nested_parameter (int a);

/**
 * RegressSkippedStructure: (skip)
 *
 * This should be skipped, and moreover, all function which
 * use it should be.
 */
typedef struct
{
  int x;
  double v;
} RegressSkippedStructure;


_GI_TEST_EXTERN
void regress_random_function_with_skipped_structure (int x,
						     RegressSkippedStructure *foo,
						     double v);

/**
 * RegressIntset:
 *
 * Like telepathy-glib's TpIntset.
 *
 */
typedef struct _RegressIntset RegressIntset;

/**
 * RegressIntsetAlias: (skip)
 *
 * Compatibility typedef, like telepathy-glib's TpIntSet
 *
 */
typedef RegressIntset RegressIntsetAlias;

/**
 * RegressPtrArrayAlias:
 *
 * Typedef'd GPtrArray for some reason
 */
typedef GPtrArray RegressPtrArrayAlias;


_GI_TEST_EXTERN
void regress_introspectable_via_alias (RegressPtrArrayAlias *data);

/**
 * RegressVaListAlias:
 *
 * Typedef'd va_list for additional reasons
 */
typedef va_list RegressVaListAlias;


_GI_TEST_EXTERN
void regress_not_introspectable_via_alias (RegressVaListAlias ok);

/**
 * RegressAliasedTestBoxed:
 *
 * Typedef TestBoxed to test caller-allocates correctness
 */
typedef RegressTestBoxed RegressAliasedTestBoxed;


_GI_TEST_EXTERN
void regress_aliased_caller_alloc (RegressAliasedTestBoxed *boxed);

/* private testing */

typedef struct {
  gint this_is_public_before;
  /* < private > */
  gint this_is_private;
  /* < public > */
  gint this_is_public_after;
} RegressTestPrivateStruct;

typedef enum {
  REGRESS_TEST_PUBLIC_ENUM_BEFORE = 1 << 0,
  /* <private> */
  REGRESS_TEST_PRIVATE_ENUM       = 1 << 1,
  /* <public> */
  REGRESS_TEST_PUBLIC_ENUM_AFTER  = 1 << 2,
} RegressTestPrivateEnum;

typedef struct {
  gint just_int;
  gint array[10];
} RegressTestStructFixedArray;


_GI_TEST_EXTERN
void regress_test_struct_fixed_array_frob (RegressTestStructFixedArray *str);

typedef struct {
  gchar name[32];
} RegressLikeXklConfigItem;


_GI_TEST_EXTERN
void regress_like_xkl_config_item_set_name (RegressLikeXklConfigItem *self,
                                            char const *name);

#define REGRESS_UTF8_CONSTANT "const \xe2\x99\xa5 utf8"

#ifdef __GNUC__
#define REGRESS_TEST_GNUC_EXTENSION_STMT(foo) (G_GNUC_EXTENSION ({ int x[10]; x[5] = 42; x[5]; }))
#endif

/* https://bugzilla.gnome.org/show_bug.cgi?id=662888 */
#define REGRESS_LONG_STRING_CONSTANT \
	"TYPE,VALUE,ENCODING,CHARSET,LANGUAGE,DOM,INTL,POSTAL,PARCEL," \
	"HOME,WORK,PREF,VOICE,FAX,MSG,CELL,PAGER,BBS,MODEM,CAR,ISDN,VIDEO," \
	"AOL,APPLELINK,ATTMAIL,CIS,EWORLD,INTERNET,IBMMAIL,MCIMAIL," \
	"POWERSHARE,PRODIGY,TLX,X400,GIF,CGM,WMF,BMP,MET,PMB,DIB,PICT,TIFF," \
	"PDF,PS,JPEG,QTIME,MPEG,MPEG2,AVI,WAVE,AIFF,PCM,X509,PGP"

_GI_TEST_EXTERN
void regress_has_parameter_named_attrs (int        foo,
                                        gpointer   attributes);

typedef struct {
  int dummy;
  struct {
    const char *name;
    guint x;
  } attributes[32];

  double dummy2;
} RegressLikeGnomeKeyringPasswordSchema;

/* Ensure we ignore symbols that start with _; in particular we don't
 * want to issue a namespace warning.
 */
#define _DONTSCANTHIS 1

/* https://bugzilla.gnome.org/show_bug.cgi?id=685022 */
#define REGRESS_MININT64 ((gint64) G_GINT64_CONSTANT(0x8000000000000000))
#define REGRESS_MAXUINT64 (G_GINT64_CONSTANT(0xffffffffffffffffU))

/* https://bugzilla.gnome.org/show_bug.cgi?id=698367 */
#ifndef __GI_SCANNER__
#define REGRESS_DONTSCANTHIS 1
#else
#define REGRESS_GI_SCANNER_ELSE 3
#endif
#ifndef BLAH
#ifdef __GI_SCANNER__
#define REGRESS_GI_SCANNER_IFDEF 3
#endif
#endif

/* This struct is one pattern by which padding can be consumed, if
 * you're willing to take a hard dependency on anonymous unions. */
typedef struct {
  int x;

  union {
    RegressLikeGnomeKeyringPasswordSchema *a[2];

    guint padding[4];
  };
} RegressAnAnonymousUnion;

typedef struct {
  int x;

  union {
    struct {
      RegressLikeGnomeKeyringPasswordSchema *a;
      RegressLikeXklConfigItem *b;
    };

    guint padding[4];
  };
} RegressAnonymousUnionAndStruct;

GVariant *
regress_get_variant (void);

typedef struct _RegressTestReferenceCounters RegressTestReferenceCounters;

struct _RegressTestReferenceCounters {
  grefcount       refcount;
  gatomicrefcount atomicrefcount;
};

_GI_TEST_EXTERN
void regress_test_array_struct_out_none (RegressTestStructA **arr, gsize *len);

_GI_TEST_EXTERN
void regress_test_array_struct_out_container (RegressTestStructA **arr, gsize *len);

_GI_TEST_EXTERN
void regress_test_array_struct_out_full_fixed (RegressTestStructA **arr);

_GI_TEST_EXTERN
void regress_test_array_struct_out_caller_alloc (RegressTestStructA *arr, gsize len);

_GI_TEST_EXTERN
void regress_test_array_struct_in_full (RegressTestStructA *arr, gsize len);

_GI_TEST_EXTERN
void regress_test_array_struct_in_none (RegressTestStructA *arr, gsize len);

_GI_TEST_EXTERN
void regress_test_obj_function_async (RegressTestObj     *self,
                                      int                 io_priority,
                                      GCancellable       *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer            user_data);

_GI_TEST_EXTERN
gboolean regress_test_obj_function_finish (RegressTestObj *self, GAsyncResult *res, GError **error);

_GI_TEST_EXTERN
gboolean regress_test_obj_function_sync (RegressTestObj *self, int io_priority);

static inline gboolean regress_test_inline_function(gboolean foo);
/*
 * regress_test_inline_function:
 *
 * An inline function that should be exposed as such
 */
static inline gboolean regress_test_inline_function (gboolean foo)
{
  return foo;
}

static inline gboolean regress_test_obj_inline_method(RegressTestObj *obj, gboolean foo);
/*
 * regress_test_obj_inline_method:
 *
 * An inline function that should be exposed as such
 */
static inline gboolean regress_test_obj_inline_method (RegressTestObj *obj, gboolean foo)
{
  return foo;
}

#endif /* __GITESTTYPES_H__ */
