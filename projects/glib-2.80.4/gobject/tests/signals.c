#include <glib-object.h>
#include "marshalers.h"

#define g_assert_cmpflags(type,n1, cmp, n2) G_STMT_START { \
                                               type __n1 = (n1), __n2 = (n2); \
                                               if (__n1 cmp __n2) ; else \
                                                 g_assertion_message_cmpint (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                             #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'i'); \
                                            } G_STMT_END
#define g_assert_cmpenum(type,n1, cmp, n2) G_STMT_START { \
                                               type __n1 = (n1), __n2 = (n2); \
                                               if (__n1 cmp __n2) ; else \
                                                 g_assertion_message_cmpint (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                             #n1 " " #cmp " " #n2, __n1, #cmp, __n2, 'i'); \
                                            } G_STMT_END

typedef enum {
  TEST_ENUM_NEGATIVE = -30,
  TEST_ENUM_NONE = 0,
  TEST_ENUM_FOO = 1,
  TEST_ENUM_BAR = 2
} TestEnum;

typedef enum {
  TEST_UNSIGNED_ENUM_FOO = 1,
  TEST_UNSIGNED_ENUM_BAR = 42
  /* Don't test 0x80000000 for now- nothing appears to do this in
   * practice, and it triggers GValue/GEnum bugs on ppc64.
   */
} TestUnsignedEnum;

static void
custom_marshal_VOID__INVOCATIONHINT (GClosure     *closure,
                                     GValue       *return_value G_GNUC_UNUSED,
                                     guint         n_param_values,
                                     const GValue *param_values,
                                     gpointer      invocation_hint,
                                     gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__INVOCATIONHINT) (gpointer     data1,
                                                     gpointer     invocation_hint,
                                                     gpointer     data2);
  GMarshalFunc_VOID__INVOCATIONHINT callback;
  GCClosure *cc = (GCClosure*) closure;
  gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__INVOCATIONHINT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            invocation_hint,
            data2);
}

static GType
test_enum_get_type (void)
{
  static GType static_g_define_type_id = 0;

  if (g_once_init_enter_pointer (&static_g_define_type_id))
    {
      static const GEnumValue values[] = {
        { TEST_ENUM_NEGATIVE, "TEST_ENUM_NEGATIVE", "negative" },
        { TEST_ENUM_NONE, "TEST_ENUM_NONE", "none" },
        { TEST_ENUM_FOO, "TEST_ENUM_FOO", "foo" },
        { TEST_ENUM_BAR, "TEST_ENUM_BAR", "bar" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("TestEnum"), values);
      g_once_init_leave_pointer (&static_g_define_type_id, g_define_type_id);
    }

  return static_g_define_type_id;
}

static GType
test_unsigned_enum_get_type (void)
{
  static GType static_g_define_type_id = 0;

  if (g_once_init_enter_pointer (&static_g_define_type_id))
    {
      static const GEnumValue values[] = {
        { TEST_UNSIGNED_ENUM_FOO, "TEST_UNSIGNED_ENUM_FOO", "foo" },
        { TEST_UNSIGNED_ENUM_BAR, "TEST_UNSIGNED_ENUM_BAR", "bar" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("TestUnsignedEnum"), values);
      g_once_init_leave_pointer (&static_g_define_type_id, g_define_type_id);
    }

  return static_g_define_type_id;
}

typedef enum {
  MY_ENUM_VALUE = 1,
} MyEnum;

static const GEnumValue my_enum_values[] =
{
  { MY_ENUM_VALUE, "the first value", "one" },
  { 0, NULL, NULL }
};

typedef enum {
  MY_FLAGS_FIRST_BIT = (1 << 0),
  MY_FLAGS_THIRD_BIT = (1 << 2),
  MY_FLAGS_LAST_BIT = (1 << 31)
} MyFlags;

static const GFlagsValue my_flag_values[] =
{
  { MY_FLAGS_FIRST_BIT, "the first bit", "first-bit" },
  { MY_FLAGS_THIRD_BIT, "the third bit", "third-bit" },
  { MY_FLAGS_LAST_BIT, "the last bit", "last-bit" },
  { 0, NULL, NULL }
};

static GType enum_type;
static GType flags_type;

static guint simple_id;
static guint simple2_id;

typedef struct {
  GTypeInterface g_iface;
} FooInterface;

GType foo_get_type (void);

G_DEFINE_INTERFACE (Foo, foo, G_TYPE_OBJECT)

static void
foo_default_init (FooInterface *iface)
{
}

typedef struct {
  GObject parent;
} Baa;

typedef struct {
  GObjectClass parent_class;
} BaaClass;

static void
baa_init_foo (FooInterface *iface)
{
}

GType baa_get_type (void);

G_DEFINE_TYPE_WITH_CODE (Baa, baa, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (foo_get_type (), baa_init_foo))

static void
baa_init (Baa *baa)
{
}

static void
baa_class_init (BaaClass *class)
{
}

typedef struct _Test Test;
typedef struct _TestClass TestClass;

struct _Test
{
  GObject parent_instance;
};

static void all_types_handler (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, MyFlags f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64);
static gboolean accumulator_sum (GSignalInvocationHint *ihint, GValue *return_accu, const GValue *handler_return, gpointer data);
static gboolean accumulator_concat_string (GSignalInvocationHint *ihint, GValue *return_accu, const GValue *handler_return, gpointer data);
static gchar * accumulator_class (Test *test);

struct _TestClass
{
  GObjectClass parent_class;

  void (* variant_changed) (Test *, GVariant *);
  void (* all_types) (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, MyFlags f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64);
  void (* all_types_null) (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, MyFlags f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64);
  gchar * (*accumulator_class) (Test *test);
};

static GType test_get_type (void);
G_DEFINE_TYPE (Test, test, G_TYPE_OBJECT)

static void
test_init (Test *test)
{
}

static void
test_class_init (TestClass *klass)
{
  guint s;

  enum_type = g_enum_register_static ("MyEnum", my_enum_values);
  flags_type = g_flags_register_static ("MyFlag", my_flag_values);

  klass->all_types = all_types_handler;
  klass->accumulator_class = accumulator_class;

  simple_id = g_signal_new ("simple",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                0);
  g_signal_new ("simple-detailed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                0);
  /* Deliberately install this one in non-canonical form to check that’s handled correctly: */
  simple2_id = g_signal_new ("simple_2",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                0);
  g_signal_new ("simple-accumulator",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                accumulator_sum, NULL,
                NULL,
                G_TYPE_INT,
                0);
  g_signal_new ("accumulator-class-first",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET (TestClass, accumulator_class),
                accumulator_concat_string, NULL,
                NULL,
                G_TYPE_STRING,
                0);
  g_signal_new ("accumulator-class-last",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, accumulator_class),
                accumulator_concat_string, NULL,
                NULL,
                G_TYPE_STRING,
                0);
  g_signal_new ("accumulator-class-cleanup",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (TestClass, accumulator_class),
                accumulator_concat_string, NULL,
                NULL,
                G_TYPE_STRING,
                0);
  g_signal_new ("accumulator-class-first-last",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, accumulator_class),
                accumulator_concat_string, NULL,
                NULL,
                G_TYPE_STRING,
                0);
  g_signal_new ("accumulator-class-first-last-cleanup",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (TestClass, accumulator_class),
                accumulator_concat_string, NULL,
                NULL,
                G_TYPE_STRING,
                0);
  g_signal_new ("accumulator-class-last-cleanup",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (TestClass, accumulator_class),
                accumulator_concat_string, NULL,
                NULL,
                G_TYPE_STRING,
                0);
  g_signal_new ("generic-marshaller-1",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                7,
                G_TYPE_CHAR, G_TYPE_UCHAR, G_TYPE_INT, G_TYPE_LONG, G_TYPE_POINTER, G_TYPE_DOUBLE, G_TYPE_FLOAT);
  g_signal_new ("generic-marshaller-2",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                5,
                G_TYPE_INT, test_enum_get_type(), G_TYPE_INT, test_unsigned_enum_get_type (), G_TYPE_INT);
  g_signal_new ("generic-marshaller-enum-return-signed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                test_enum_get_type(),
                0);
  g_signal_new ("generic-marshaller-enum-return-unsigned",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                test_unsigned_enum_get_type(),
                0);
  g_signal_new ("generic-marshaller-int-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_INT,
                0);
  s = g_signal_new ("va-marshaller-int-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                test_INT__VOID,
                G_TYPE_INT,
                0);
  g_signal_set_va_marshaller (s, G_TYPE_FROM_CLASS (klass),
			      test_INT__VOIDv);
  g_signal_new ("generic-marshaller-uint-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_UINT,
                0);
  g_signal_new ("generic-marshaller-interface-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                NULL,
                foo_get_type (),
                0);
  s = g_signal_new ("va-marshaller-uint-return",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                test_INT__VOID,
                G_TYPE_UINT,
                0);
  g_signal_set_va_marshaller (s, G_TYPE_FROM_CLASS (klass),
			      test_UINT__VOIDv);
  g_signal_new ("custom-marshaller",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                custom_marshal_VOID__INVOCATIONHINT,
                G_TYPE_NONE,
                1,
                G_TYPE_POINTER);
  g_signal_new ("variant-changed-no-slot",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VARIANT,
                G_TYPE_NONE,
                1,
                G_TYPE_VARIANT);
  g_signal_new ("variant-changed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                G_STRUCT_OFFSET (TestClass, variant_changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__VARIANT,
                G_TYPE_NONE,
                1,
                G_TYPE_VARIANT);
  g_signal_new ("all-types",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, all_types),
                NULL, NULL,
                test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
  s = g_signal_new ("all-types-va",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, all_types),
                NULL, NULL,
                test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
  g_signal_set_va_marshaller (s, G_TYPE_FROM_CLASS (klass),
			      test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64v);

  g_signal_new ("all-types-generic",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, all_types),
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
  g_signal_new ("all-types-null",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (TestClass, all_types_null),
                NULL, NULL,
                test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
  g_signal_new ("all-types-empty",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0,
                NULL, NULL,
                test_VOID__INT_BOOLEAN_CHAR_UCHAR_UINT_LONG_ULONG_ENUM_FLAGS_FLOAT_DOUBLE_STRING_PARAM_BOXED_POINTER_OBJECT_VARIANT_INT64_UINT64,
                G_TYPE_NONE,
                19,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		enum_type,
		flags_type,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_PARAM_LONG,
		G_TYPE_BYTES,
		G_TYPE_POINTER,
		test_get_type (),
                G_TYPE_VARIANT,
		G_TYPE_INT64,
		G_TYPE_UINT64);
}

typedef struct _Test Test2;
typedef struct _TestClass Test2Class;

static GType test2_get_type (void);
G_DEFINE_TYPE (Test2, test2, G_TYPE_OBJECT)

static void
test2_init (Test2 *test)
{
}

static void
test2_class_init (Test2Class *klass)
{
}

static void
test_variant_signal (void)
{
  Test *test;
  GVariant *v;

  /* Tests that the signal emission consumes the variant,
   * even if there are no handlers connected.
   */

  test = g_object_new (test_get_type (), NULL);

  v = g_variant_new_boolean (TRUE);
  g_variant_ref (v);
  g_assert_true (g_variant_is_floating (v));
  g_signal_emit_by_name (test, "variant-changed-no-slot", v);
  g_assert_false (g_variant_is_floating (v));
  g_variant_unref (v);

  v = g_variant_new_boolean (TRUE);
  g_variant_ref (v);
  g_assert_true (g_variant_is_floating (v));
  g_signal_emit_by_name (test, "variant-changed", v);
  g_assert_false (g_variant_is_floating (v));
  g_variant_unref (v);

  g_object_unref (test);
}

static void
on_generic_marshaller_1 (Test *obj,
			 gint8 v_schar,
			 guint8 v_uchar,
			 gint v_int,
			 glong v_long,
			 gpointer v_pointer,
			 gdouble v_double,
			 gfloat v_float,
			 gpointer user_data)
{
  g_assert_cmpint (v_schar, ==, 42);
  g_assert_cmpint (v_uchar, ==, 43);
  g_assert_cmpint (v_int, ==, 4096);
  g_assert_cmpint (v_long, ==, 8192);
  g_assert_null (v_pointer);
  g_assert_cmpfloat (v_double, >, 0.0);
  g_assert_cmpfloat (v_double, <, 1.0);
  g_assert_cmpfloat (v_float, >, 5.0);
  g_assert_cmpfloat (v_float, <, 6.0);
}
			 
static void
test_generic_marshaller_signal_1 (void)
{
  Test *test;
  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test, "generic-marshaller-1", G_CALLBACK (on_generic_marshaller_1), NULL);

  g_signal_emit_by_name (test, "generic-marshaller-1", 42, 43, 4096, 8192, NULL, 0.5, 5.5);

  g_object_unref (test);
}

static void
on_generic_marshaller_2 (Test *obj,
			 gint        v_int1,
			 TestEnum    v_enum,
			 gint        v_int2,
			 TestUnsignedEnum v_uenum,
			 gint        v_int3)
{
  g_assert_cmpint (v_int1, ==, 42);
  g_assert_cmpint (v_enum, ==, TEST_ENUM_BAR);
  g_assert_cmpint (v_int2, ==, 43);
  g_assert_cmpint (v_uenum, ==, TEST_UNSIGNED_ENUM_BAR);
  g_assert_cmpint (v_int3, ==, 44);
}

static void
test_generic_marshaller_signal_2 (void)
{
  Test *test;
  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test, "generic-marshaller-2", G_CALLBACK (on_generic_marshaller_2), NULL);

  g_signal_emit_by_name (test, "generic-marshaller-2", 42, TEST_ENUM_BAR, 43, TEST_UNSIGNED_ENUM_BAR, 44);

  g_object_unref (test);
}

static TestEnum
on_generic_marshaller_enum_return_signed_1 (Test *obj)
{
  return TEST_ENUM_NEGATIVE;
}

static TestEnum
on_generic_marshaller_enum_return_signed_2 (Test *obj)
{
  return TEST_ENUM_BAR;
}

static void
test_generic_marshaller_signal_enum_return_signed (void)
{
  Test *test;
  guint id;
  TestEnum retval = 0;

  test = g_object_new (test_get_type (), NULL);

  /* Test return value NEGATIVE */
  id = g_signal_connect (test,
                         "generic-marshaller-enum-return-signed",
                         G_CALLBACK (on_generic_marshaller_enum_return_signed_1),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-enum-return-signed", &retval);
  g_assert_cmpint (retval, ==, TEST_ENUM_NEGATIVE);
  g_signal_handler_disconnect (test, id);

  /* Test return value BAR */
  retval = 0;
  id = g_signal_connect (test,
                         "generic-marshaller-enum-return-signed",
                         G_CALLBACK (on_generic_marshaller_enum_return_signed_2),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-enum-return-signed", &retval);
  g_assert_cmpint (retval, ==, TEST_ENUM_BAR);
  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

static TestUnsignedEnum
on_generic_marshaller_enum_return_unsigned_1 (Test *obj)
{
  return TEST_UNSIGNED_ENUM_FOO;
}

static TestUnsignedEnum
on_generic_marshaller_enum_return_unsigned_2 (Test *obj)
{
  return TEST_UNSIGNED_ENUM_BAR;
}

static void
test_generic_marshaller_signal_enum_return_unsigned (void)
{
  Test *test;
  guint id;
  TestUnsignedEnum retval = 0;

  test = g_object_new (test_get_type (), NULL);

  /* Test return value FOO */
  id = g_signal_connect (test,
                         "generic-marshaller-enum-return-unsigned",
                         G_CALLBACK (on_generic_marshaller_enum_return_unsigned_1),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-enum-return-unsigned", &retval);
  g_assert_cmpint (retval, ==, TEST_UNSIGNED_ENUM_FOO);
  g_signal_handler_disconnect (test, id);

  /* Test return value BAR */
  retval = 0;
  id = g_signal_connect (test,
                         "generic-marshaller-enum-return-unsigned",
                         G_CALLBACK (on_generic_marshaller_enum_return_unsigned_2),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-enum-return-unsigned", &retval);
  g_assert_cmpint (retval, ==, TEST_UNSIGNED_ENUM_BAR);
  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

/**********************/

static gint
on_generic_marshaller_int_return_signed_1 (Test *obj)
{
  return -30;
}

static gint
on_generic_marshaller_int_return_signed_2 (Test *obj)
{
  return 2;
}

static void
test_generic_marshaller_signal_int_return (void)
{
  Test *test;
  guint id;
  gint retval = 0;

  test = g_object_new (test_get_type (), NULL);

  /* Test return value -30 */
  id = g_signal_connect (test,
                         "generic-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, -30);
  g_signal_handler_disconnect (test, id);

  /* Test return value positive */
  retval = 0;
  id = g_signal_connect (test,
                         "generic-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_2),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, 2);
  g_signal_handler_disconnect (test, id);

  /* Same test for va marshaller */

  /* Test return value -30 */
  id = g_signal_connect (test,
                         "va-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                         NULL);
  g_signal_emit_by_name (test, "va-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, -30);
  g_signal_handler_disconnect (test, id);

  /* Test return value positive */
  retval = 0;
  id = g_signal_connect (test,
                         "va-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_2),
                         NULL);
  g_signal_emit_by_name (test, "va-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, 2);
  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

static guint
on_generic_marshaller_uint_return_1 (Test *obj)
{
  return 1;
}

static guint
on_generic_marshaller_uint_return_2 (Test *obj)
{
  return G_MAXUINT;
}

static void
test_generic_marshaller_signal_uint_return (void)
{
  Test *test;
  guint id;
  guint retval = 0;

  test = g_object_new (test_get_type (), NULL);

  id = g_signal_connect (test,
                         "generic-marshaller-uint-return",
                         G_CALLBACK (on_generic_marshaller_uint_return_1),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-uint-return", &retval);
  g_assert_cmpint (retval, ==, 1);
  g_signal_handler_disconnect (test, id);

  retval = 0;
  id = g_signal_connect (test,
                         "generic-marshaller-uint-return",
                         G_CALLBACK (on_generic_marshaller_uint_return_2),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-uint-return", &retval);
  g_assert_cmpint (retval, ==, G_MAXUINT);
  g_signal_handler_disconnect (test, id);

  /* Same test for va marshaller */

  id = g_signal_connect (test,
                         "va-marshaller-uint-return",
                         G_CALLBACK (on_generic_marshaller_uint_return_1),
                         NULL);
  g_signal_emit_by_name (test, "va-marshaller-uint-return", &retval);
  g_assert_cmpint (retval, ==, 1);
  g_signal_handler_disconnect (test, id);

  retval = 0;
  id = g_signal_connect (test,
                         "va-marshaller-uint-return",
                         G_CALLBACK (on_generic_marshaller_uint_return_2),
                         NULL);
  g_signal_emit_by_name (test, "va-marshaller-uint-return", &retval);
  g_assert_cmpint (retval, ==, G_MAXUINT);
  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

static gpointer
on_generic_marshaller_interface_return (Test *test)
{
  return g_object_new (baa_get_type (), NULL);
}

static void
test_generic_marshaller_signal_interface_return (void)
{
  Test *test;
  guint id;
  gpointer retval;

  test = g_object_new (test_get_type (), NULL);

  /* Test return value -30 */
  id = g_signal_connect (test,
                         "generic-marshaller-interface-return",
                         G_CALLBACK (on_generic_marshaller_interface_return),
                         NULL);
  g_signal_emit_by_name (test, "generic-marshaller-interface-return", &retval);
  g_assert_true (g_type_check_instance_is_a ((GTypeInstance*)retval, foo_get_type ()));
  g_object_unref (retval);

  g_signal_handler_disconnect (test, id);

  g_object_unref (test);
}

static const GSignalInvocationHint dont_use_this = { 0, };

static void
custom_marshaller_callback (Test                  *test,
                            GSignalInvocationHint *hint,
                            gpointer               unused)
{
  GSignalInvocationHint *ihint;

  g_assert_true (hint != &dont_use_this);

  ihint = g_signal_get_invocation_hint (test);

  g_assert_cmpuint (hint->signal_id, ==, ihint->signal_id);
  g_assert_cmpuint (hint->detail , ==, ihint->detail);
  g_assert_cmpflags (GSignalFlags, hint->run_type, ==, ihint->run_type); 
}

static void
test_custom_marshaller (void)
{
  Test *test;

  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test,
                    "custom-marshaller",
                    G_CALLBACK (custom_marshaller_callback),
                    NULL);

  g_signal_emit_by_name (test, "custom-marshaller", &dont_use_this);

  g_object_unref (test);
}

static int all_type_handlers_count = 0;

static void
all_types_handler (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, MyFlags f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64)
{
  all_type_handlers_count++;

  g_assert_cmpint (i, ==, 42);
  g_assert_cmpint (b, ==, TRUE);
  g_assert_cmpint (c, ==, 17);
  g_assert_cmpuint (uc, ==, 140);
  g_assert_cmpuint (ui, ==, G_MAXUINT - 42);
  g_assert_cmpint (l, ==, -1117);
  g_assert_cmpuint (ul, ==, G_MAXULONG - 999);
  g_assert_cmpenum (MyEnum, e, ==, MY_ENUM_VALUE);
  g_assert_cmpflags (MyFlags, f, ==, MY_FLAGS_FIRST_BIT | MY_FLAGS_THIRD_BIT | MY_FLAGS_LAST_BIT);
  g_assert_cmpfloat (fl, ==, 0.25);
  g_assert_cmpfloat (db, ==, 1.5);
  g_assert_cmpstr (str, ==, "Test");
  g_assert_cmpstr (g_param_spec_get_nick (param), ==, "nick");
  g_assert_cmpstr (g_bytes_get_data (bytes, NULL), ==, "Blah");
  g_assert_true (ptr == &enum_type);
  g_assert_cmpuint (g_variant_get_uint16 (var), == , 99);
  g_assert_cmpint (i64, ==, G_MAXINT64 - 1234);
  g_assert_cmpuint (ui64, ==, G_MAXUINT64 - 123456);
}

static void
all_types_handler_cb (Test *test, int i, gboolean b, char c, guchar uc, guint ui, glong l, gulong ul, MyEnum e, guint f, float fl, double db, char *str, GParamSpec *param, GBytes *bytes, gpointer ptr, Test *obj, GVariant *var, gint64 i64, guint64 ui64, gpointer user_data)
{
  g_assert_true (user_data == &flags_type);
  all_types_handler (test, i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, obj, var, i64, ui64);
}

static void
test_all_types (void)
{
  Test *test;

  int i = 42;
  gboolean b = TRUE;
  char c = 17;
  guchar uc = 140;
  guint ui = G_MAXUINT - 42;
  glong l =  -1117;
  gulong ul = G_MAXULONG - 999;
  MyEnum e = MY_ENUM_VALUE;
  MyFlags f = MY_FLAGS_FIRST_BIT | MY_FLAGS_THIRD_BIT | MY_FLAGS_LAST_BIT;
  float fl = 0.25;
  double db = 1.5;
  char *str = "Test";
  GParamSpec *param = g_param_spec_long	 ("param", "nick", "blurb", 0, 10, 4, 0);
  GBytes *bytes = g_bytes_new_static ("Blah", 5);
  gpointer ptr = &enum_type;
  GVariant *var = g_variant_new_uint16 (99);
  gint64 i64;
  guint64 ui64;
  g_variant_ref_sink (var);
  i64 = G_MAXINT64 - 1234;
  ui64 = G_MAXUINT64 - 123456;

  test = g_object_new (test_get_type (), NULL);

  all_type_handlers_count = 0;

  g_signal_emit_by_name (test, "all-types",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-va",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-generic",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-empty",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-null",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);

  g_assert_cmpint (all_type_handlers_count, ==, 3);

  all_type_handlers_count = 0;

  g_signal_connect (test, "all-types", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-va", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-generic", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-empty", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-null", G_CALLBACK (all_types_handler_cb), &flags_type);

  g_signal_emit_by_name (test, "all-types",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-va",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-generic",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-empty",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-null",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);

  g_assert_cmpint (all_type_handlers_count, ==, 3 + 5);

  all_type_handlers_count = 0;

  g_signal_connect (test, "all-types", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-va", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-generic", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-empty", G_CALLBACK (all_types_handler_cb), &flags_type);
  g_signal_connect (test, "all-types-null", G_CALLBACK (all_types_handler_cb), &flags_type);

  g_signal_emit_by_name (test, "all-types",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-va",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-generic",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-empty",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);
  g_signal_emit_by_name (test, "all-types-null",
			 i, b, c, uc, ui, l, ul, e, f, fl, db, str, param, bytes, ptr, test, var, i64, ui64);

  g_assert_cmpint (all_type_handlers_count, ==, 3 + 5 + 5);

  g_object_unref (test);
  g_param_spec_unref (param);
  g_bytes_unref (bytes);
  g_variant_unref (var);
}

static void
test_connect (void)
{
  GObject *test;
  gint retval;

  test = g_object_new (test_get_type (), NULL);

  g_object_connect (test,
                    "signal::generic-marshaller-int-return",
                    G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                    NULL,
                    "object-signal::va-marshaller-int-return",
                    G_CALLBACK (on_generic_marshaller_int_return_signed_2),
                    NULL,
                    NULL);
  g_signal_emit_by_name (test, "generic-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, -30);
  g_signal_emit_by_name (test, "va-marshaller-int-return", &retval);
  g_assert_cmpint (retval, ==, 2);

  g_object_disconnect (test,
                       "any-signal",
                       G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                       NULL,
                       "any-signal::va-marshaller-int-return",
                       G_CALLBACK (on_generic_marshaller_int_return_signed_2),
                       NULL,
                       NULL);

  g_object_unref (test);
}

static void
simple_handler1 (GObject *sender,
                 GObject *target)
{
  g_object_unref (target);
}

static void
simple_handler2 (GObject *sender,
                 GObject *target)
{
  g_object_unref (target);
}

static void
test_destroy_target_object (void)
{
  Test *sender, *target1, *target2;

  sender = g_object_new (test_get_type (), NULL);
  target1 = g_object_new (test_get_type (), NULL);
  target2 = g_object_new (test_get_type (), NULL);
  g_signal_connect_object (sender, "simple", G_CALLBACK (simple_handler1),
                           target1, G_CONNECT_DEFAULT);
  g_signal_connect_object (sender, "simple", G_CALLBACK (simple_handler2),
                           target2, G_CONNECT_DEFAULT);
  g_signal_emit_by_name (sender, "simple");
  g_object_unref (sender);
}

static gboolean
hook_func (GSignalInvocationHint *ihint,
           guint                  n_params,
           const GValue          *params,
           gpointer               data)
{
  gint *count = data;

  (*count)++;

  return TRUE;
}

static gboolean
hook_func_removal (GSignalInvocationHint *ihint,
                   guint                  n_params,
                   const GValue          *params,
                   gpointer               data)
{
  gint *count = data;

  (*count)++;

  return FALSE;
}

static void
simple_handler_remove_hook (GObject *sender,
                            gpointer data)
{
  gulong *hook = data;

  g_signal_remove_emission_hook (simple_id, *hook);
}

static void
test_emission_hook (void)
{
  GObject *test1, *test2;
  gint count = 0;
  gulong hook;
  gulong connection_id;

  test1 = g_object_new (test_get_type (), NULL);
  test2 = g_object_new (test_get_type (), NULL);

  hook = g_signal_add_emission_hook (simple_id, 0, hook_func, &count, NULL);
  g_assert_cmpint (count, ==, 0);
  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 1);
  g_signal_emit_by_name (test2, "simple");
  g_assert_cmpint (count, ==, 2);
  g_signal_remove_emission_hook (simple_id, hook);
  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 2);

  count = 0;
  hook = g_signal_add_emission_hook (simple_id, 0, hook_func_removal, &count, NULL);
  g_assert_cmpint (count, ==, 0);
  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 1);
  g_signal_emit_by_name (test2, "simple");
  g_assert_cmpint (count, ==, 1);

  g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                         "*simple* had no hook * to remove");
  g_signal_remove_emission_hook (simple_id, hook);
  g_test_assert_expected_messages ();

  count = 0;
  hook = g_signal_add_emission_hook (simple_id, 0, hook_func, &count, NULL);
  connection_id = g_signal_connect (test1, "simple",
                                    G_CALLBACK (simple_handler_remove_hook), &hook);
  g_assert_cmpint (count, ==, 0);
  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 1);
  g_signal_emit_by_name (test2, "simple");
  g_assert_cmpint (count, ==, 1);

  g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                         "*simple* had no hook * to remove");
  g_signal_remove_emission_hook (simple_id, hook);
  g_test_assert_expected_messages ();

  g_clear_signal_handler (&connection_id, test1);

  gulong hooks[10];
  count = 0;

  for (size_t i = 0; i < G_N_ELEMENTS (hooks); ++i)
    hooks[i] = g_signal_add_emission_hook (simple_id, 0, hook_func, &count, NULL);

  g_assert_cmpint (count, ==, 0);
  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 10);
  g_signal_emit_by_name (test2, "simple");
  g_assert_cmpint (count, ==, 20);

  for (size_t i = 0; i < G_N_ELEMENTS (hooks); ++i)
    g_signal_remove_emission_hook (simple_id, hooks[i]);

  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 20);

  count = 0;

  for (size_t i = 0; i < G_N_ELEMENTS (hooks); ++i)
    hooks[i] = g_signal_add_emission_hook (simple_id, 0, hook_func_removal, &count, NULL);

  g_assert_cmpint (count, ==, 0);
  g_signal_emit_by_name (test1, "simple");
  g_assert_cmpint (count, ==, 10);
  g_signal_emit_by_name (test2, "simple");
  g_assert_cmpint (count, ==, 10);

  for (size_t i = 0; i < G_N_ELEMENTS (hooks); ++i)
    {
      g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                         "*simple* had no hook * to remove");
      g_signal_remove_emission_hook (simple_id, hooks[i]);
      g_test_assert_expected_messages ();
    }

  g_object_unref (test1);
  g_object_unref (test2);
}

static void
simple_cb (gpointer instance, gpointer data)
{
  GSignalInvocationHint *ihint;

  ihint = g_signal_get_invocation_hint (instance);

  g_assert_cmpstr (g_signal_name (ihint->signal_id), ==, "simple");

  g_signal_emit_by_name (instance, "simple-2");
}

static void
simple2_cb (gpointer instance, gpointer data)
{
  GSignalInvocationHint *ihint;

  ihint = g_signal_get_invocation_hint (instance);

  g_assert_cmpstr (g_signal_name (ihint->signal_id), ==, "simple-2");
}

static void
test_invocation_hint (void)
{
  GObject *test;

  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test, "simple", G_CALLBACK (simple_cb), NULL);
  g_signal_connect (test, "simple-2", G_CALLBACK (simple2_cb), NULL);
  g_signal_emit_by_name (test, "simple");

  g_object_unref (test);
}

static gboolean
accumulator_sum (GSignalInvocationHint *ihint,
                 GValue                *return_accu,
                 const GValue          *handler_return,
                 gpointer               data)
{
  gint acc = g_value_get_int (return_accu);
  gint ret = g_value_get_int (handler_return);

  g_assert_cmpint (ret, >, 0);

  if (ihint->run_type & G_SIGNAL_ACCUMULATOR_FIRST_RUN)
    {
      g_assert_cmpint (acc, ==, 0);
      g_assert_cmpint (ret, ==, 1);
      g_assert_true (ihint->run_type & G_SIGNAL_RUN_FIRST);
      g_assert_false (ihint->run_type & G_SIGNAL_RUN_LAST);
    }
  else if (ihint->run_type & G_SIGNAL_RUN_FIRST)
    {
      /* Only the first signal handler was called so far */
      g_assert_cmpint (acc, ==, 1);
      g_assert_cmpint (ret, ==, 2);
      g_assert_false (ihint->run_type & G_SIGNAL_RUN_LAST);
    }
  else if (ihint->run_type & G_SIGNAL_RUN_LAST)
    {
      /* Only the first two signal handler were called so far */
      g_assert_cmpint (acc, ==, 3);
      g_assert_cmpint (ret, ==, 3);
      g_assert_false (ihint->run_type & G_SIGNAL_RUN_FIRST);
    }
  else
    {
      g_assert_not_reached ();
    }

  g_value_set_int (return_accu, acc + ret);

  /* Continue with the other signal handlers as long as the sum is < 6,
   * i.e. don't run simple_accumulator_4_cb() */
  return acc + ret < 6;
}

static gint
simple_accumulator_1_cb (gpointer instance, gpointer data)
{
  return 1;
}

static gint
simple_accumulator_2_cb (gpointer instance, gpointer data)
{
  return 2;
}

static gint
simple_accumulator_3_cb (gpointer instance, gpointer data)
{
  return 3;
}

static gint
simple_accumulator_4_cb (gpointer instance, gpointer data)
{
  return 4;
}

static void
test_accumulator (void)
{
  GObject *test;
  gint ret = -1;

  test = g_object_new (test_get_type (), NULL);

  /* Connect in reverse order to make sure that LAST signal handlers are
   * called after FIRST signal handlers but signal handlers in each "group"
   * are called in the order they were registered */
  g_signal_connect_after (test, "simple-accumulator", G_CALLBACK (simple_accumulator_3_cb), NULL);
  g_signal_connect_after (test, "simple-accumulator", G_CALLBACK (simple_accumulator_4_cb), NULL);
  g_signal_connect (test, "simple-accumulator", G_CALLBACK (simple_accumulator_1_cb), NULL);
  g_signal_connect (test, "simple-accumulator", G_CALLBACK (simple_accumulator_2_cb), NULL);
  g_signal_emit_by_name (test, "simple-accumulator", &ret);

  /* simple_accumulator_4_cb() is not run because accumulator is 6 */
  g_assert_cmpint (ret, ==, 6);

  g_object_unref (test);
}

static gboolean
accumulator_concat_string (GSignalInvocationHint *ihint, GValue *return_accu, const GValue *handler_return, gpointer data)
{
  const gchar *acc = g_value_get_string (return_accu);
  const gchar *ret = g_value_get_string (handler_return);

  g_assert_nonnull (ret);

  if (acc == NULL)
    g_value_set_string (return_accu, ret);
  else
    g_value_take_string (return_accu, g_strconcat (acc, ret, NULL));

  return TRUE;
}

static gchar *
accumulator_class_before_cb (gpointer instance, gpointer data)
{
  return g_strdup ("before");
}

static gchar *
accumulator_class_after_cb (gpointer instance, gpointer data)
{
  return g_strdup ("after");
}

static gchar *
accumulator_class (Test *test)
{
  return g_strdup ("class");
}

static void
test_accumulator_class (void)
{
  const struct {
    const gchar *signal_name;
    const gchar *return_string;
  } tests[] = {
    {"accumulator-class-first", "classbeforeafter"},
    {"accumulator-class-last", "beforeclassafter"},
    {"accumulator-class-cleanup", "beforeafterclass"},
    {"accumulator-class-first-last", "classbeforeclassafter"},
    {"accumulator-class-first-last-cleanup", "classbeforeclassafterclass"},
    {"accumulator-class-last-cleanup", "beforeclassafterclass"},
  };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      GObject *test;
      gchar *ret = NULL;

      g_test_message ("Signal: %s", tests[i].signal_name);

      test = g_object_new (test_get_type (), NULL);

      g_signal_connect (test, tests[i].signal_name, G_CALLBACK (accumulator_class_before_cb), NULL);
      g_signal_connect_after (test, tests[i].signal_name, G_CALLBACK (accumulator_class_after_cb), NULL);
      g_signal_emit_by_name (test, tests[i].signal_name, &ret);

      g_assert_cmpstr (ret, ==, tests[i].return_string);
      g_free (ret);

      g_object_unref (test);
    }
}

static gboolean
in_set (const gchar *s,
        const gchar *set[])
{
  gint i;

  for (i = 0; set[i]; i++)
    {
      if (g_strcmp0 (s, set[i]) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
test_introspection (void)
{
  guint *ids;
  guint n_ids;
  const gchar *name;
  guint i;
  const gchar *names[] = {
    "simple",
    "simple-detailed",
    "simple-2",
    "simple-accumulator",
    "accumulator-class-first",
    "accumulator-class-last",
    "accumulator-class-cleanup",
    "accumulator-class-first-last",
    "accumulator-class-first-last-cleanup",
    "accumulator-class-last-cleanup",
    "generic-marshaller-1",
    "generic-marshaller-2",
    "generic-marshaller-enum-return-signed",
    "generic-marshaller-enum-return-unsigned",
    "generic-marshaller-int-return",
    "va-marshaller-int-return",
    "generic-marshaller-uint-return",
    "generic-marshaller-interface-return",
    "va-marshaller-uint-return",
    "variant-changed-no-slot",
    "variant-changed",
    "all-types",
    "all-types-va",
    "all-types-generic",
    "all-types-null",
    "all-types-empty",
    "custom-marshaller",
    NULL
  };
  GSignalQuery query;

  ids = g_signal_list_ids (test_get_type (), &n_ids);
  g_assert_cmpuint (n_ids, ==, g_strv_length ((gchar**)names));

  for (i = 0; i < n_ids; i++)
    {
      name = g_signal_name (ids[i]);
      g_assert_true (in_set (name, names));
    }

  g_signal_query (simple_id, &query);
  g_assert_cmpuint (query.signal_id, ==, simple_id);
  g_assert_cmpstr (query.signal_name, ==, "simple");
  g_assert_true (query.itype == test_get_type ());
  g_assert_cmpint (query.signal_flags, ==, G_SIGNAL_RUN_LAST);
  g_assert_cmpint ((uintptr_t) query.return_type, ==, (uintptr_t) G_TYPE_NONE);
  g_assert_cmpuint (query.n_params, ==, 0);

  g_free (ids);
}

static void
test_handler (gpointer instance, gpointer data)
{
  gint *count = data;

  (*count)++;
}

static void
test_block_handler (void)
{
  GObject *test1, *test2;
  gint count1 = 0;
  gint count2 = 0;
  gulong handler1, handler;

  test1 = g_object_new (test_get_type (), NULL);
  test2 = g_object_new (test_get_type (), NULL);

  handler1 = g_signal_connect (test1, "simple", G_CALLBACK (test_handler), &count1);
  g_signal_connect (test2, "simple", G_CALLBACK (test_handler), &count2);

  handler = g_signal_handler_find (test1, G_SIGNAL_MATCH_ID, simple_id, 0, NULL, NULL, NULL);

  g_assert_true (handler == handler1);

  g_assert_cmpint (count1, ==, 0);
  g_assert_cmpint (count2, ==, 0);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 1);
  g_assert_cmpint (count2, ==, 1);

  g_signal_handler_block (test1, handler1);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 1);
  g_assert_cmpint (count2, ==, 2);

  g_signal_handler_unblock (test1, handler1);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 2);
  g_assert_cmpint (count2, ==, 3);

  g_assert_cmpuint (g_signal_handlers_block_matched (test1, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, test_block_handler, NULL), ==, 0);
  g_assert_cmpuint (g_signal_handlers_block_matched (test2, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, test_handler, NULL), ==, 1);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 3);
  g_assert_cmpint (count2, ==, 3);

  g_signal_handlers_unblock_matched (test2, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, test_handler, NULL);

  /* Test match by signal ID. */
  g_assert_cmpuint (g_signal_handlers_block_matched (test1, G_SIGNAL_MATCH_ID, simple_id, 0, NULL, NULL, NULL), ==, 1);

  g_signal_emit_by_name (test1, "simple");
  g_signal_emit_by_name (test2, "simple");

  g_assert_cmpint (count1, ==, 3);
  g_assert_cmpint (count2, ==, 4);

  g_assert_cmpuint (g_signal_handlers_unblock_matched (test1, G_SIGNAL_MATCH_ID, simple_id, 0, NULL, NULL, NULL), ==, 1);

  /* Match types are conjunctive */
  g_assert_cmpuint (g_signal_handlers_block_matched (test1, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, NULL, test_handler, "will not match"), ==, 0);
  g_assert_cmpuint (g_signal_handlers_block_matched (test1, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, NULL, test_handler, &count1), ==, 1);
  g_assert_cmpuint (g_signal_handlers_unblock_matched (test1, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, NULL, test_handler, &count1), ==, 1);

  /* Test g_signal_handlers_disconnect_matched for G_SIGNAL_MATCH_ID match */
  g_assert_cmpuint (g_signal_handlers_disconnect_matched (test1,
                                                          G_SIGNAL_MATCH_ID,
                                                          simple_id, 0,
                                                          NULL, NULL, NULL),
                    ==,
                    1);
  g_assert_cmpuint (g_signal_handler_find (test1,
                                           G_SIGNAL_MATCH_ID,
                                           simple_id, 0,
                                           NULL, NULL, NULL),
                    ==,
                    0);

  g_object_unref (test1);
  g_object_unref (test2);
}

static void
stop_emission (gpointer instance, gpointer data)
{
  g_signal_stop_emission (instance, simple_id, 0);
}

static void
stop_emission_by_name (gpointer instance, gpointer data)
{
  g_signal_stop_emission_by_name (instance, "simple");
}

static void
dont_reach (gpointer instance, gpointer data)
{
  g_assert_not_reached ();
}

static void
test_stop_emission (void)
{
  GObject *test1;
  gulong handler;

  test1 = g_object_new (test_get_type (), NULL);
  handler = g_signal_connect (test1, "simple", G_CALLBACK (stop_emission), NULL);
  g_signal_connect_after (test1, "simple", G_CALLBACK (dont_reach), NULL);

  g_signal_emit_by_name (test1, "simple");

  g_signal_handler_disconnect (test1, handler);
  g_signal_connect (test1, "simple", G_CALLBACK (stop_emission_by_name), NULL);

  g_signal_emit_by_name (test1, "simple");

  g_object_unref (test1);
}

static void
test_signal_disconnect_wrong_object (void)
{
  Test *object, *object2;
  Test2 *object3;
  guint signal_id;

  object = g_object_new (test_get_type (), NULL);
  object2 = g_object_new (test_get_type (), NULL);
  object3 = g_object_new (test2_get_type (), NULL);

  signal_id = g_signal_connect (object,
                                "simple",
                                G_CALLBACK (simple_handler1),
                                NULL);

  /* disconnect from the wrong object (same type), should warn */
  g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                         "*: instance '*' has no handler with id '*'");
  g_signal_handler_disconnect (object2, signal_id);
  g_test_assert_expected_messages ();

  /* and from an object of the wrong type */
  g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL,
                         "*: instance '*' has no handler with id '*'");
  g_signal_handler_disconnect (object3, signal_id);
  g_test_assert_expected_messages ();

  /* it's still connected */
  g_assert_true (g_signal_handler_is_connected (object, signal_id));

  g_object_unref (object);
  g_object_unref (object2);
  g_object_unref (object3);
}

static void
test_clear_signal_handler (void)
{
  GObject *test_obj;
  gulong handler;

  test_obj = g_object_new (test_get_type (), NULL);

  handler = g_signal_connect (test_obj, "simple", G_CALLBACK (dont_reach), NULL);
  g_assert_cmpuint (handler, >, 0);

  g_clear_signal_handler (&handler, test_obj);
  g_assert_cmpuint (handler, ==, 0);

  g_signal_emit_by_name (test_obj, "simple");

  g_clear_signal_handler (&handler, test_obj);

  if (g_test_undefined ())
    {
      handler = g_random_int_range (0x01, 0xFF);
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*instance '* has no handler with id *'");
      g_clear_signal_handler (&handler, test_obj);
      g_assert_cmpuint (handler, ==, 0);
      g_test_assert_expected_messages ();
    }

  g_object_unref (test_obj);
}

static void
test_lookup (void)
{
  GTypeClass *test_class;
  guint signal_id, saved_signal_id;

  g_test_summary ("Test that g_signal_lookup() works with a variety of inputs.");

  test_class = g_type_class_ref (test_get_type ());

  signal_id = g_signal_lookup ("all-types", test_get_type ());
  g_assert_cmpint (signal_id, !=, 0);

  saved_signal_id = signal_id;

  /* Try with a non-canonical name. */
  signal_id = g_signal_lookup ("all_types", test_get_type ());
  g_assert_cmpint (signal_id, ==, saved_signal_id);

  /* Looking up a non-existent signal should return nothing. */
  g_assert_cmpint (g_signal_lookup ("nope", test_get_type ()), ==, 0);

  g_type_class_unref (test_class);
}

static void
test_lookup_invalid (void)
{
  g_test_summary ("Test that g_signal_lookup() emits a warning if looking up an invalid signal name.");

  if (g_test_subprocess ())
    {
      GTypeClass *test_class;
      guint signal_id;

      test_class = g_type_class_ref (test_get_type ());

      signal_id = g_signal_lookup ("", test_get_type ());
      g_assert_cmpint (signal_id, ==, 0);

      g_type_class_unref (test_class);
      return;
    }

  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*unable to look up invalid signal name*");
}

static void
test_parse_name (void)
{
  GTypeClass *test_class;
  guint signal_id, saved_signal_id;
  gboolean retval;
  GQuark detail, saved_detail;

  g_test_summary ("Test that g_signal_parse_name() works with a variety of inputs.");

  test_class = g_type_class_ref (test_get_type ());

  /* Simple test. */
  retval = g_signal_parse_name ("simple-detailed", test_get_type (), &signal_id, &detail, TRUE);
  g_assert_true (retval);
  g_assert_cmpint (signal_id, !=, 0);
  g_assert_cmpint (detail, ==, 0);

  saved_signal_id = signal_id;

  /* Simple test with detail. */
  retval = g_signal_parse_name ("simple-detailed::a-detail", test_get_type (), &signal_id, &detail, TRUE);
  g_assert_true (retval);
  g_assert_cmpint (signal_id, ==, saved_signal_id);
  g_assert_cmpint (detail, !=, 0);

  saved_detail = detail;

  /* Simple test with the same detail again. */
  retval = g_signal_parse_name ("simple-detailed::a-detail", test_get_type (), &signal_id, &detail, FALSE);
  g_assert_true (retval);
  g_assert_cmpint (signal_id, ==, saved_signal_id);
  g_assert_cmpint (detail, ==, saved_detail);

  /* Simple test with a new detail. */
  retval = g_signal_parse_name ("simple-detailed::another-detail", test_get_type (), &signal_id, &detail, FALSE);
  g_assert_true (retval);
  g_assert_cmpint (signal_id, ==, saved_signal_id);
  g_assert_cmpint (detail, ==, 0);  /* we didn’t force the quark */

  /* Canonicalisation shouldn’t affect the results. */
  retval = g_signal_parse_name ("simple_detailed::a-detail", test_get_type (), &signal_id, &detail, FALSE);
  g_assert_true (retval);
  g_assert_cmpint (signal_id, ==, saved_signal_id);
  g_assert_cmpint (detail, ==, saved_detail);

  /* Details don’t have to look like property names. */
  retval = g_signal_parse_name ("simple-detailed::hello::world", test_get_type (), &signal_id, &detail, TRUE);
  g_assert_true (retval);
  g_assert_cmpint (signal_id, ==, saved_signal_id);
  g_assert_cmpint (detail, !=, 0);

  /* Trying to parse a detail for a signal which isn’t %G_SIGNAL_DETAILED should fail. */
  retval = g_signal_parse_name ("all-types::a-detail", test_get_type (), &signal_id, &detail, FALSE);
  g_assert_false (retval);

  g_type_class_unref (test_class);
}

static void
test_parse_name_invalid (void)
{
  GTypeClass *test_class;
  gsize i;
  guint signal_id;
  GQuark detail;
  const gchar *vectors[] =
    {
      "",
      "7zip",
      "invalid:signal",
      "simple-detailed::",
      "simple-detailed:",
      ":",
      "::",
      ":valid-detail",
      "::valid-detail",
    };

  g_test_summary ("Test that g_signal_parse_name() ignores a variety of invalid inputs.");

  test_class = g_type_class_ref (test_get_type ());

  for (i = 0; i < G_N_ELEMENTS (vectors); i++)
    {
      g_test_message ("Parser input: %s", vectors[i]);
      g_assert_false (g_signal_parse_name (vectors[i], test_get_type (), &signal_id, &detail, TRUE));
    }

  g_type_class_unref (test_class);
}

static void
test_signals_invalid_name (gconstpointer test_data)
{
  const gchar *signal_name = test_data;

  g_test_summary ("Check that g_signal_new() rejects invalid signal names.");

  if (g_test_subprocess ())
    {
      g_signal_new (signal_name,
                    test_get_type (),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE,
                    0);
      return;
    }

  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*g_signal_is_valid_name (signal_name)*");
}

static void
test_signal_is_valid_name (void)
{
  const gchar *valid_names[] =
    {
      "signal",
      "i",
      "multiple-segments",
      "segment0-SEGMENT1",
      "using_underscores",
    };
  const gchar *invalid_names[] =
    {
      "",
      "7zip",
      "my_int:hello",
    };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (valid_names); i++)
    g_assert_true (g_signal_is_valid_name (valid_names[i]));

  for (i = 0; i < G_N_ELEMENTS (invalid_names); i++)
    g_assert_false (g_signal_is_valid_name (invalid_names[i]));
}

static void
test_emitv (void)
{
  GArray *values;
  GObject *test;
  GValue return_value = G_VALUE_INIT;
  gint count = 0;
  guint signal_id;
  gulong hook;
  gulong id;

  test = g_object_new (test_get_type (), NULL);

  values = g_array_new (TRUE, TRUE, sizeof (GValue));
  g_array_set_clear_func (values, (GDestroyNotify) g_value_unset);

  g_array_set_size (values, 1);
  g_value_init (&g_array_index (values, GValue, 0), G_TYPE_OBJECT);
  g_value_set_object (&g_array_index (values, GValue, 0), test);
  hook = g_signal_add_emission_hook (simple_id, 0, hook_func, &count, NULL);
  g_assert_cmpint (count, ==, 0);
  g_signal_emitv ((GValue *) values->data, simple_id, 0, NULL);
  g_assert_cmpint (count, ==, 1);
  g_signal_remove_emission_hook (simple_id, hook);

  g_array_set_size (values, 20);
  g_value_init (&g_array_index (values, GValue, 1), G_TYPE_INT);
  g_value_set_int (&g_array_index (values, GValue, 1), 42);

  g_value_init (&g_array_index (values, GValue, 2), G_TYPE_BOOLEAN);
  g_value_set_boolean (&g_array_index (values, GValue, 2), TRUE);

  g_value_init (&g_array_index (values, GValue, 3), G_TYPE_CHAR);
  g_value_set_schar (&g_array_index (values, GValue, 3), 17);

  g_value_init (&g_array_index (values, GValue, 4), G_TYPE_UCHAR);
  g_value_set_uchar (&g_array_index (values, GValue, 4), 140);

  g_value_init (&g_array_index (values, GValue, 5), G_TYPE_UINT);
  g_value_set_uint (&g_array_index (values, GValue, 5), G_MAXUINT - 42);

  g_value_init (&g_array_index (values, GValue, 6), G_TYPE_LONG);
  g_value_set_long (&g_array_index (values, GValue, 6), -1117);

  g_value_init (&g_array_index (values, GValue, 7), G_TYPE_ULONG);
  g_value_set_ulong (&g_array_index (values, GValue, 7), G_MAXULONG - 999);

  g_value_init (&g_array_index (values, GValue, 8), enum_type);
  g_value_set_enum (&g_array_index (values, GValue, 8), MY_ENUM_VALUE);

  g_value_init (&g_array_index (values, GValue, 9), flags_type);
  g_value_set_flags (&g_array_index (values, GValue, 9),
                     MY_FLAGS_FIRST_BIT | MY_FLAGS_THIRD_BIT | MY_FLAGS_LAST_BIT);

  g_value_init (&g_array_index (values, GValue, 10), G_TYPE_FLOAT);
  g_value_set_float (&g_array_index (values, GValue, 10), 0.25);

  g_value_init (&g_array_index (values, GValue, 11), G_TYPE_DOUBLE);
  g_value_set_double (&g_array_index (values, GValue, 11), 1.5);

  g_value_init (&g_array_index (values, GValue, 12), G_TYPE_STRING);
  g_value_set_string (&g_array_index (values, GValue, 12), "Test");

  g_value_init (&g_array_index (values, GValue, 13), G_TYPE_PARAM_LONG);
  g_value_take_param (&g_array_index (values, GValue, 13),
                      g_param_spec_long	 ("param", "nick", "blurb", 0, 10, 4, 0));

  g_value_init (&g_array_index (values, GValue, 14), G_TYPE_BYTES);
  g_value_take_boxed (&g_array_index (values, GValue, 14),
                      g_bytes_new_static ("Blah", 5));

  g_value_init (&g_array_index (values, GValue, 15), G_TYPE_POINTER);
  g_value_set_pointer (&g_array_index (values, GValue, 15), &enum_type);

  g_value_init (&g_array_index (values, GValue, 16), test_get_type ());
  g_value_set_object (&g_array_index (values, GValue, 16), test);

  g_value_init (&g_array_index (values, GValue, 17), G_TYPE_VARIANT);
  g_value_take_variant (&g_array_index (values, GValue, 17),
                        g_variant_ref_sink (g_variant_new_uint16 (99)));

  g_value_init (&g_array_index (values, GValue, 18), G_TYPE_INT64);
  g_value_set_int64 (&g_array_index (values, GValue, 18), G_MAXINT64 - 1234);

  g_value_init (&g_array_index (values, GValue, 19), G_TYPE_UINT64);
  g_value_set_uint64 (&g_array_index (values, GValue, 19), G_MAXUINT64 - 123456);

  id = g_signal_connect (test, "all-types", G_CALLBACK (all_types_handler_cb), &flags_type);
  signal_id = g_signal_lookup ("all-types", test_get_type ());
  g_assert_cmpuint (signal_id, >, 0);

  count = 0;
  hook = g_signal_add_emission_hook (signal_id, 0, hook_func, &count, NULL);
  g_assert_cmpint (count, ==, 0);
  g_signal_emitv ((GValue *) values->data, signal_id, 0, NULL);
  g_assert_cmpint (count, ==, 1);
  g_signal_remove_emission_hook (signal_id, hook);
  g_clear_signal_handler (&id, test);


  signal_id = g_signal_lookup ("generic-marshaller-int-return", test_get_type ());
  g_assert_cmpuint (signal_id, >, 0);
  g_array_set_size (values, 1);

  id = g_signal_connect (test,
                         "generic-marshaller-int-return",
                         G_CALLBACK (on_generic_marshaller_int_return_signed_1),
                         NULL);

  count = 0;
  hook = g_signal_add_emission_hook (signal_id, 0, hook_func, &count, NULL);
  g_assert_cmpint (count, ==, 0);
  g_value_init (&return_value, G_TYPE_INT);
  g_signal_emitv ((GValue *) values->data, signal_id, 0, &return_value);
  g_assert_cmpint (count, ==, 1);
  g_assert_cmpint (g_value_get_int (&return_value), ==, -30);
  g_signal_remove_emission_hook (signal_id, hook);
  g_clear_signal_handler (&id, test);

#ifdef G_ENABLE_DEBUG
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*return*value*generic-marshaller-int-return*NULL*");
  g_signal_emitv ((GValue *) values->data, signal_id, 0, NULL);
  g_test_assert_expected_messages ();

  g_value_unset (&return_value);
  g_value_init (&return_value, G_TYPE_FLOAT);
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*return*value*generic-marshaller-int-return*gfloat*");
  g_signal_emitv ((GValue *) values->data, signal_id, 0, &return_value);
  g_test_assert_expected_messages ();
#endif

  g_object_unref (test);
  g_array_unref (values);
}

typedef struct
{
  GWeakRef wr;
  gulong handler;
} TestWeakRefDisconnect;

static void
weak_ref_disconnect_notify (gpointer  data,
                            GObject  *where_object_was)
{
  TestWeakRefDisconnect *state = data;
  g_assert_null (g_weak_ref_get (&state->wr));
  state->handler = 0;
}

static void
test_weak_ref_disconnect (void)
{
  TestWeakRefDisconnect state;
  GObject *test;

  test = g_object_new (test_get_type (), NULL);
  g_weak_ref_init (&state.wr, test);
  state.handler = g_signal_connect_data (test,
                                         "simple",
                                         G_CALLBACK (dont_reach),
                                         &state,
                                         (GClosureNotify) weak_ref_disconnect_notify,
                                         0);
  g_assert_cmpint (state.handler, >, 0);

  g_object_unref (test);

  g_assert_cmpint (state.handler, ==, 0);
  g_assert_null (g_weak_ref_get (&state.wr));
  g_weak_ref_clear (&state.wr);
}

/* --- */

int
main (int argc,
     char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gobject/signals/all-types", test_all_types);
  g_test_add_func ("/gobject/signals/variant", test_variant_signal);
  g_test_add_func ("/gobject/signals/destroy-target-object", test_destroy_target_object);
  g_test_add_func ("/gobject/signals/generic-marshaller-1", test_generic_marshaller_signal_1);
  g_test_add_func ("/gobject/signals/generic-marshaller-2", test_generic_marshaller_signal_2);
  g_test_add_func ("/gobject/signals/generic-marshaller-enum-return-signed", test_generic_marshaller_signal_enum_return_signed);
  g_test_add_func ("/gobject/signals/generic-marshaller-enum-return-unsigned", test_generic_marshaller_signal_enum_return_unsigned);
  g_test_add_func ("/gobject/signals/generic-marshaller-int-return", test_generic_marshaller_signal_int_return);
  g_test_add_func ("/gobject/signals/generic-marshaller-uint-return", test_generic_marshaller_signal_uint_return);
  g_test_add_func ("/gobject/signals/generic-marshaller-interface-return", test_generic_marshaller_signal_interface_return);
  g_test_add_func ("/gobject/signals/custom-marshaller", test_custom_marshaller);
  g_test_add_func ("/gobject/signals/connect", test_connect);
  g_test_add_func ("/gobject/signals/emission-hook", test_emission_hook);
  g_test_add_func ("/gobject/signals/emitv", test_emitv);
  g_test_add_func ("/gobject/signals/accumulator", test_accumulator);
  g_test_add_func ("/gobject/signals/accumulator-class", test_accumulator_class);
  g_test_add_func ("/gobject/signals/introspection", test_introspection);
  g_test_add_func ("/gobject/signals/block-handler", test_block_handler);
  g_test_add_func ("/gobject/signals/stop-emission", test_stop_emission);
  g_test_add_func ("/gobject/signals/invocation-hint", test_invocation_hint);
  g_test_add_func ("/gobject/signals/test-disconnection-wrong-object", test_signal_disconnect_wrong_object);
  g_test_add_func ("/gobject/signals/clear-signal-handler", test_clear_signal_handler);
  g_test_add_func ("/gobject/signals/lookup", test_lookup);
  g_test_add_func ("/gobject/signals/lookup/invalid", test_lookup_invalid);
  g_test_add_func ("/gobject/signals/parse-name", test_parse_name);
  g_test_add_func ("/gobject/signals/parse-name/invalid", test_parse_name_invalid);
  g_test_add_data_func ("/gobject/signals/invalid-name/colon", "my_int:hello", test_signals_invalid_name);
  g_test_add_data_func ("/gobject/signals/invalid-name/first-char", "7zip", test_signals_invalid_name);
  g_test_add_data_func ("/gobject/signals/invalid-name/empty", "", test_signals_invalid_name);
  g_test_add_func ("/gobject/signals/is-valid-name", test_signal_is_valid_name);
  g_test_add_func ("/gobject/signals/weak-ref-disconnect", test_weak_ref_disconnect);

  return g_test_run ();
}
