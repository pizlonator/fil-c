#include <glib-object.h>

static void
test_registration_serial (void)
{
  gint serial1, serial2, serial3;

  serial1 = g_type_get_type_registration_serial ();
  g_pointer_type_register_static ("my+pointer");
  serial2 = g_type_get_type_registration_serial ();
  g_assert (serial1 != serial2);
  serial3 = g_type_get_type_registration_serial ();
  g_assert (serial2 == serial3);
}

typedef struct {
  GTypeInterface g_iface;
} BarInterface;

GType bar_get_type (void);

G_DEFINE_INTERFACE (Bar, bar, G_TYPE_OBJECT)

static void
bar_default_init (BarInterface *iface)
{
}

typedef struct {
  GTypeInterface g_iface;
} FooInterface;

GType foo_get_type (void);

G_DEFINE_INTERFACE_WITH_CODE (Foo, foo, G_TYPE_OBJECT,
                              g_type_interface_add_prerequisite (g_define_type_id, bar_get_type ()))

static void
foo_default_init (FooInterface *iface)
{
}

typedef struct {
  GTypeInterface g_iface;
} BaaInterface;

GType baa_get_type (void);

G_DEFINE_INTERFACE (Baa, baa, G_TYPE_INVALID)

static void
baa_default_init (BaaInterface *iface)
{
}

typedef struct {
  GTypeInterface g_iface;
} BooInterface;

GType boo_get_type (void);

G_DEFINE_INTERFACE_WITH_CODE (Boo, boo, G_TYPE_INVALID,
                              g_type_interface_add_prerequisite (g_define_type_id, baa_get_type ()))

static void
boo_default_init (BooInterface *iface)
{
}

typedef struct {
  GTypeInterface g_iface;
} BibiInterface;

GType bibi_get_type (void);

G_DEFINE_INTERFACE (Bibi, bibi, G_TYPE_INITIALLY_UNOWNED)

static void
bibi_default_init (BibiInterface *iface)
{
}

typedef struct {
  GTypeInterface g_iface;
} BozoInterface;

GType bozo_get_type (void);

G_DEFINE_INTERFACE_WITH_CODE (Bozo, bozo, G_TYPE_INVALID,
                              g_type_interface_add_prerequisite (g_define_type_id, foo_get_type ());
                              g_type_interface_add_prerequisite (g_define_type_id, bibi_get_type ()))

static void
bozo_default_init (BozoInterface *iface)
{
}



static void
test_interface_prerequisite (void)
{
  GType *prereqs;
  guint n_prereqs;
  gpointer iface;
  gpointer parent;

  prereqs = g_type_interface_prerequisites (foo_get_type (), &n_prereqs);
  g_assert_cmpint (n_prereqs, ==, 2);
  g_assert (prereqs[0] == bar_get_type ());
  g_assert (prereqs[1] == G_TYPE_OBJECT);
  g_assert (g_type_interface_instantiatable_prerequisite (foo_get_type ()) == G_TYPE_OBJECT);

  iface = g_type_default_interface_ref (foo_get_type ());
  parent = g_type_interface_peek_parent (iface);
  g_assert (parent == NULL);
  g_type_default_interface_unref (iface);

  g_free (prereqs);

  g_assert_cmpint ((uintptr_t) g_type_interface_instantiatable_prerequisite (baa_get_type ()), ==, (uintptr_t) G_TYPE_INVALID);
  g_assert_cmpint ((uintptr_t) g_type_interface_instantiatable_prerequisite (boo_get_type ()), ==, (uintptr_t) G_TYPE_INVALID);

  g_assert_cmpint ((uintptr_t) g_type_interface_instantiatable_prerequisite (bozo_get_type ()), ==, (uintptr_t) G_TYPE_INITIALLY_UNOWNED);
}

typedef struct {
  GTypeInterface g_iface;
} BazInterface;

GType baz_get_type (void);

G_DEFINE_INTERFACE (Baz, baz, G_TYPE_OBJECT)

static void
baz_default_init (BazInterface *iface)
{
}

typedef struct {
  GObject parent;
} Bazo;

typedef struct {
  GObjectClass parent_class;
} BazoClass;

GType bazo_get_type (void);
static void bazo_iface_init (BazInterface *i);

G_DEFINE_TYPE_WITH_CODE (Bazo, bazo, G_TYPE_INITIALLY_UNOWNED,
                         G_IMPLEMENT_INTERFACE (baz_get_type (),
                                                bazo_iface_init);)

static void
bazo_init (Bazo *b)
{
}

static void
bazo_class_init (BazoClass *c)
{
}

static void
bazo_iface_init (BazInterface *i)
{
}

static gint check_called;

static void
check_func (gpointer check_data,
            gpointer g_iface)
{
  g_assert (check_data == &check_called);

  check_called++;
}

static void
test_interface_check (void)
{
  GObject *o;

  check_called = 0;
  g_type_add_interface_check (&check_called, check_func);
  o = g_object_ref_sink (g_object_new (bazo_get_type (), NULL));
  g_object_unref (o);
  g_assert_cmpint (check_called, ==, 1);
  g_type_remove_interface_check (&check_called, check_func);
}

static void
test_next_base (void)
{
  GType type;

  type = g_type_next_base (bazo_get_type (), G_TYPE_OBJECT);

  g_assert (type == G_TYPE_INITIALLY_UNOWNED);
}

/* Test that the macro an function versions of g_type_is_a
 * work the same
 */
static void
test_is_a (void)
{
  g_assert_true (g_type_is_a (G_TYPE_OBJECT, G_TYPE_OBJECT));
  g_assert_true ((g_type_is_a) (G_TYPE_OBJECT, G_TYPE_OBJECT));
  g_assert_true (g_type_is_a (bar_get_type (), G_TYPE_OBJECT));
  g_assert_true ((g_type_is_a) (bar_get_type (), G_TYPE_OBJECT));
  g_assert_false (g_type_is_a (bar_get_type (), bibi_get_type ()));
  g_assert_false ((g_type_is_a) (bar_get_type (), bibi_get_type ()));
}

static void
test_query (void)
{
  GTypeQuery results;

  g_test_message ("Invalid types can’t be queried.");
  g_type_query (G_TYPE_INVALID, &results);
  g_assert_cmpuint ((uintptr_t) results.type, ==, 0);

  g_test_message ("Unclassed types can’t be queried.");
  g_type_query (G_TYPE_INT64, &results);
  g_assert_cmpuint ((uintptr_t) results.type, ==, 0);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/type/registration-serial", test_registration_serial);
  g_test_add_func ("/type/interface-prerequisite", test_interface_prerequisite);
  g_test_add_func ("/type/interface-check", test_interface_check);
  g_test_add_func ("/type/next-base", test_next_base);
  g_test_add_func ("/type/is-a", test_is_a);
  g_test_add_func ("/type/query", test_query);

  return g_test_run ();
}
