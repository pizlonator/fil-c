#include <glib-object.h>

static void
test_fundamentals (void)
{
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_NONE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INTERFACE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_CHAR));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UCHAR));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_BOOLEAN));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UINT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_LONG));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_ULONG));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INT64));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UINT64));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_ENUM));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_FLAGS));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_FLOAT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_DOUBLE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_STRING));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_POINTER));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_BOXED));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_PARAM));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_OBJECT));
  g_assert (G_TYPE_OBJECT == g_object_get_type ());
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_VARIANT));
  g_assert (G_TYPE_IS_DERIVED (G_TYPE_INITIALLY_UNOWNED));

  g_assert (g_type_fundamental_next () == G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST));
}

static void
test_type_qdata (void)
{
  gchar *data;

  g_type_set_qdata (G_TYPE_ENUM, g_quark_from_string ("bla"), "bla");
  data = g_type_get_qdata (G_TYPE_ENUM, g_quark_from_string ("bla"));
  g_assert_cmpstr (data, ==, "bla");
}

static void
test_type_query (void)
{
  GTypeQuery query;

  g_type_query (G_TYPE_ENUM, &query);
  g_assert_cmpint ((uintptr_t) query.type, ==, (uintptr_t) G_TYPE_ENUM);
  g_assert_cmpstr (query.type_name, ==, "GEnum");
  g_assert_cmpint (query.class_size, ==, sizeof (GEnumClass));
  g_assert_cmpint (query.instance_size, ==, 0);
}

typedef struct _MyObject MyObject;
typedef struct _MyObjectClass MyObjectClass;
typedef struct _MyObjectClassPrivate MyObjectClassPrivate;

struct _MyObject
{
  GObject parent_instance;

  gint count;
};

struct _MyObjectClass
{
  GObjectClass parent_class;
};

struct _MyObjectClassPrivate
{
  gint secret_class_count;
};

static GType my_object_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyObject, my_object, G_TYPE_OBJECT,
                         g_type_add_class_private (g_define_type_id, sizeof (MyObjectClassPrivate)) );

static void
my_object_init (MyObject *obj)
{
  obj->count = 42;
}

static void
my_object_class_init (MyObjectClass *klass)
{
}

static void
test_class_private (void)
{
  GObject *obj;
  MyObjectClass *class;
  MyObjectClassPrivate *priv;

  obj = g_object_new (my_object_get_type (), NULL);

  class = g_type_class_ref (my_object_get_type ());
  priv = G_TYPE_CLASS_GET_PRIVATE (class, my_object_get_type (), MyObjectClassPrivate);
  priv->secret_class_count = 13;
  g_type_class_unref (class);

  g_object_unref (obj);

  g_assert_cmpint (g_type_qname (my_object_get_type ()), ==, g_quark_from_string ("MyObject"));
}

static void
test_clear (void)
{
  GObject *o = NULL;
  GObject *tmp;

  g_clear_object (&o);
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  o = g_object_ref (tmp);
  g_assert (o != NULL);

  g_assert_cmpint (tmp->ref_count, ==, 2);
  g_clear_object (&o);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  g_assert (o == NULL);

  g_object_unref (tmp);
}

static void
test_clear_function (void)
{
  GObject *o = NULL;
  GObject *tmp;

  (g_clear_object) (&o);
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  o = g_object_ref (tmp);
  g_assert (o != NULL);

  g_assert_cmpint (tmp->ref_count, ==, 2);
  (g_clear_object) (&o);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  g_assert (o == NULL);

  g_object_unref (tmp);
}

static void
test_set (void)
{
  GObject *o = NULL;
  GObject *tmp;
  gpointer tmp_weak = NULL;

  g_assert (!g_set_object (&o, NULL));
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  tmp_weak = tmp;
  g_object_add_weak_pointer (tmp, &tmp_weak);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  g_assert (g_set_object (&o, tmp));
  g_assert (o == tmp);
  g_assert_cmpint (tmp->ref_count, ==, 2);

  g_object_unref (tmp);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  /* Setting it again shouldn’t cause finalisation. */
  g_assert (!g_set_object (&o, tmp));
  g_assert (o == tmp);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  g_assert_nonnull (tmp_weak);

  g_assert (g_set_object (&o, NULL));
  g_assert (o == NULL);
  g_assert_null (tmp_weak);
}

static void
test_set_function (void)
{
  GObject *o = NULL;
  GObject *tmp;
  gpointer tmp_weak = NULL;

  g_assert (!(g_set_object) (&o, NULL));
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  tmp_weak = tmp;
  g_object_add_weak_pointer (tmp, &tmp_weak);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  g_assert ((g_set_object) (&o, tmp));
  g_assert (o == tmp);
  g_assert_cmpint (tmp->ref_count, ==, 2);

  g_object_unref (tmp);
  g_assert_cmpint (tmp->ref_count, ==, 1);

  /* Setting it again shouldn’t cause finalisation. */
  g_assert (!(g_set_object) (&o, tmp));
  g_assert (o == tmp);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  g_assert_nonnull (tmp_weak);

  g_assert ((g_set_object) (&o, NULL));
  g_assert (o == NULL);
  g_assert_null (tmp_weak);
}

static void
test_set_derived_type (void)
{
  GBinding *obj = NULL;
  GObject *o = NULL;
  GBinding *b = NULL;

  g_test_summary ("Check that g_set_object() doesn’t give strict aliasing "
                  "warnings when used on types derived from GObject");

  g_assert_false (g_set_object (&o, NULL));
  g_assert_null (o);

  g_assert_false (g_set_object (&b, NULL));
  g_assert_null (b);

  obj = g_object_new (my_object_get_type (), NULL);

  g_assert_true (g_set_object (&o, G_OBJECT (obj)));
  g_assert_true (o == G_OBJECT (obj));

  g_assert_true (g_set_object (&b, obj));
  g_assert_true (b == obj);

  g_object_unref (obj);
  g_clear_object (&b);
  g_clear_object (&o);
}

static void
toggle_cb (gpointer data, GObject *obj, gboolean is_last)
{
  gboolean *b = data;

  *b = TRUE;
}

static void
test_object_value (void)
{
  GObject *v;
  GObject *v2;
  GValue value = G_VALUE_INIT;
  gboolean toggled = FALSE;

  g_value_init (&value, G_TYPE_OBJECT);

  v = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_add_toggle_ref (v, toggle_cb, &toggled);

  g_value_take_object (&value, v);

  v2 = g_value_get_object (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_object (&value);
  g_assert (v2 == v);  /* objects use ref/unref for copy/free */
  g_object_unref (v2);

  g_assert (!toggled);
  g_value_unset (&value);
  g_assert (toggled);

  /* test the deprecated variant too */
  g_value_init (&value, G_TYPE_OBJECT);
  /* get a new reference */
  g_object_ref (v);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_value_set_object_take_ownership (&value, v);
G_GNUC_END_IGNORE_DEPRECATIONS

  toggled = FALSE;
  g_value_unset (&value);
  g_assert (toggled);

  g_object_remove_toggle_ref (v, toggle_cb, &toggled);
}

static void
test_initially_unowned (void)
{
  GObject *obj;

  obj = g_object_new (G_TYPE_INITIALLY_UNOWNED, NULL);
  g_assert (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_assert (!g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_assert (!g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 2);

  g_object_unref (obj);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_force_floating (obj);
  g_assert (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_object_unref (obj);

  obj = g_object_new (G_TYPE_INITIALLY_UNOWNED, NULL);
  g_assert_true (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_take_ref (obj);
  g_assert_false (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_take_ref (obj);
  g_assert_false (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_unref (obj);

  if (g_test_undefined ())
    {
      obj = g_object_new (G_TYPE_INITIALLY_UNOWNED, NULL);

#ifdef G_ENABLE_DEBUG
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "A floating object GInitiallyUnowned * was finalized*");
#endif
      g_object_unref (obj);
#ifdef G_ENABLE_DEBUG
      g_test_assert_expected_messages ();
#endif
    }
}

static void
test_weak_pointer (void)
{
  GObject *obj;
  gpointer weak;
  gpointer weak2;

  weak = weak2 = obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_add_weak_pointer (obj, &weak);
  g_object_add_weak_pointer (obj, &weak2);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert (weak == obj);
  g_assert (weak2 == obj);

  g_object_remove_weak_pointer (obj, &weak2);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert (weak == obj);
  g_assert (weak2 == obj);

  g_object_unref (obj);
  g_assert (weak == NULL);
  g_assert (weak2 == obj);
}

static void
test_weak_pointer_clear (void)
{
  GObject *obj;
  gpointer weak = NULL;

  g_clear_weak_pointer (&weak);
  g_assert_null (weak);

  weak = obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_add_weak_pointer (obj, &weak);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_clear_weak_pointer (&weak);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_null (weak);

  g_object_unref (obj);
}

static void
test_weak_pointer_clear_function (void)
{
  GObject *obj;
  gpointer weak = NULL;

  (g_clear_weak_pointer) (&weak);
  g_assert_null (weak);

  weak = obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_add_weak_pointer (obj, &weak);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  (g_clear_weak_pointer) (&weak);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_null (weak);

  g_object_unref (obj);
}

static void
test_weak_pointer_set (void)
{
  GObject *obj;
  gpointer weak = NULL;

  g_assert_false (g_set_weak_pointer (&weak, NULL));
  g_assert_null (weak);

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_assert_true (g_set_weak_pointer (&weak, obj));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_assert_true (g_set_weak_pointer (&weak, NULL));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_null (weak);

  g_assert_true (g_set_weak_pointer (&weak, obj));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_object_unref (obj);
  g_assert_null (weak);
}

static void
test_weak_pointer_set_function (void)
{
  GObject *obj;
  gpointer weak = NULL;

  g_assert_false ((g_set_weak_pointer) (&weak, NULL));
  g_assert_null (weak);

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_assert_true ((g_set_weak_pointer) (&weak, obj));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_assert_true ((g_set_weak_pointer) (&weak, NULL));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_null (weak);

  g_assert_true ((g_set_weak_pointer) (&weak, obj));
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert_true (weak == obj);

  g_object_unref (obj);
  g_assert_null (weak);
}

/* See gobject/tests/threadtests.c for the threaded version */
static void
test_weak_ref (void)
{
  GObject *obj;
  GObject *obj2;
  GObject *tmp;
  GWeakRef weak = { { GUINT_TO_POINTER (0xDEADBEEFU) } };
  GWeakRef weak2 = { { GUINT_TO_POINTER (0xDEADBEEFU) } };
  GWeakRef weak3 = { { GUINT_TO_POINTER (0xDEADBEEFU) } };
  GWeakRef *dynamic_weak = g_new (GWeakRef, 1);

  /* you can initialize to empty like this... */
  g_weak_ref_init (&weak2, NULL);
  g_assert (g_weak_ref_get (&weak2) == NULL);

  /* ... or via an initializer */
  g_weak_ref_init (&weak3, NULL);
  g_assert (g_weak_ref_get (&weak3) == NULL);

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  obj2 = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj2->ref_count, ==, 1);

  /* you can init with an object (even if uninitialized) */
  g_weak_ref_init (&weak, obj);
  g_weak_ref_init (dynamic_weak, obj);
  /* or set to point at an object, if initialized (maybe to 0) */
  g_weak_ref_set (&weak2, obj);
  g_weak_ref_set (&weak3, obj);
  /* none of this affects its refcount */
  g_assert_cmpint (obj->ref_count, ==, 1);

  /* getting the value takes a ref */
  tmp = g_weak_ref_get (&weak);
  g_assert (tmp == obj);
  g_assert_cmpint (obj->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj->ref_count, ==, 1);

  tmp = g_weak_ref_get (&weak2);
  g_assert (tmp == obj);
  g_assert_cmpint (obj->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj->ref_count, ==, 1);

  tmp = g_weak_ref_get (&weak3);
  g_assert (tmp == obj);
  g_assert_cmpint (obj->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj->ref_count, ==, 1);

  tmp = g_weak_ref_get (dynamic_weak);
  g_assert (tmp == obj);
  g_assert_cmpint (obj->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj->ref_count, ==, 1);

  /* clearing a weak ref stops tracking */
  g_weak_ref_clear (&weak);

  /* setting a weak ref to NULL stops tracking too */
  g_weak_ref_set (&weak2, NULL);
  g_assert (g_weak_ref_get (&weak2) == NULL);
  g_weak_ref_clear (&weak2);

  /* setting a weak ref to a new object stops tracking the old one */
  g_weak_ref_set (dynamic_weak, obj2);
  tmp = g_weak_ref_get (dynamic_weak);
  g_assert (tmp == obj2);
  g_assert_cmpint (obj2->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj2->ref_count, ==, 1);

  g_assert_cmpint (obj->ref_count, ==, 1);

  /* free the object: weak3 is the only one left pointing there */
  g_object_unref (obj);
  g_assert (g_weak_ref_get (&weak3) == NULL);

  /* setting a weak ref to a new object stops tracking the old one */
  g_weak_ref_set (dynamic_weak, obj2);
  tmp = g_weak_ref_get (dynamic_weak);
  g_assert (tmp == obj2);
  g_assert_cmpint (obj2->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj2->ref_count, ==, 1);

  g_weak_ref_clear (&weak3);

  /* unset dynamic_weak... */
  g_weak_ref_set (dynamic_weak, NULL);
  g_assert_null (g_weak_ref_get (dynamic_weak));

  /* initializing a weak reference to an object that had before works */
  g_weak_ref_set (dynamic_weak, obj2);
  tmp = g_weak_ref_get (dynamic_weak);
  g_assert_true (tmp == obj2);
  g_assert_cmpint (obj2->ref_count, ==, 2);
  g_object_unref (tmp);
  g_assert_cmpint (obj2->ref_count, ==, 1);

  /* clear and free dynamic_weak... */
  g_weak_ref_clear (dynamic_weak);

  /* ... to prove that doing so stops this from being a use-after-free */
  g_object_unref (obj2);
  g_free (dynamic_weak);
}

G_DECLARE_FINAL_TYPE (WeakReffedObject, weak_reffed_object,
                      WEAK, REFFED_OBJECT, GObject)

struct _WeakReffedObject
{
  GObject parent;

  GWeakRef *weak_ref;
};

G_DEFINE_TYPE (WeakReffedObject, weak_reffed_object, G_TYPE_OBJECT)

static void
weak_reffed_object_dispose (GObject *object)
{
  WeakReffedObject *weak_reffed = WEAK_REFFED_OBJECT (object);

  g_assert_cmpint (object->ref_count, ==, 1);

  g_weak_ref_set (weak_reffed->weak_ref, object);

  G_OBJECT_CLASS (weak_reffed_object_parent_class)->dispose (object);

  g_assert_true (object == g_weak_ref_get (weak_reffed->weak_ref));
  g_object_unref (object);
}

static void
weak_reffed_object_init (WeakReffedObject *connector)
{
}

static void
weak_reffed_object_class_init (WeakReffedObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = weak_reffed_object_dispose;
}

static void
test_weak_ref_on_dispose (void)
{
  WeakReffedObject *obj;
  GWeakRef weak = { { GUINT_TO_POINTER (0xDEADBEEFU) } };

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/issues/2390");
  g_test_summary ("Test that a weak ref set during dispose vfunc is cleared");

  g_weak_ref_init (&weak, NULL);

  obj = g_object_new (weak_reffed_object_get_type (), NULL);
  obj->weak_ref = &weak;

  g_assert_cmpint (G_OBJECT (obj)->ref_count, ==, 1);
  g_clear_object (&obj);

  g_assert_null (g_weak_ref_get (&weak));
}

static void
test_weak_ref_on_run_dispose (void)
{
  GObject *obj;
  GWeakRef weak = { { GUINT_TO_POINTER (0xDEADBEEFU) } };

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/issues/865");
  g_test_summary ("Test that a weak ref is cleared on g_object_run_dispose()");

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_weak_ref_init (&weak, obj);

  g_assert_true (obj == g_weak_ref_get (&weak));
  g_object_unref (obj);

  g_object_run_dispose (obj);
  g_assert_null (g_weak_ref_get (&weak));

  g_weak_ref_set (&weak, obj);

  g_clear_object (&obj);
  g_assert_null (g_weak_ref_get (&weak));
}

static void
on_weak_ref_toggle_notify (gpointer data,
                           GObject *object,
                           gboolean is_last_ref)
{
  GWeakRef *weak = data;

  if (is_last_ref)
    g_weak_ref_set (weak, object);
}

static void
on_weak_ref_toggle_notify_disposed (gpointer data,
                                    GObject *object)
{
  g_assert_cmpint (object->ref_count, ==, 1);

  g_object_ref (object);
  g_object_unref (object);
}

static void
test_weak_ref_on_toggle_notify (void)
{
  GObject *obj;
  GWeakRef weak = { { GUINT_TO_POINTER (0xDEADBEEFU) } };

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/issues/2390");
  g_test_summary ("Test that a weak ref set on toggle notify is cleared");

  g_weak_ref_init (&weak, NULL);

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_add_toggle_ref (obj, on_weak_ref_toggle_notify, &weak);
  g_object_weak_ref (obj, on_weak_ref_toggle_notify_disposed, NULL);
  g_object_unref (obj);

  g_assert_cmpint (obj->ref_count, ==, 1);
  g_clear_object (&obj);

  g_assert_null (g_weak_ref_get (&weak));
}

static void
weak_ref_in_toggle_notify_toggle_cb (gpointer data,
                                     GObject *object,
                                     gboolean is_last_ref)
{
  GWeakRef weak2;
  GObject *obj2;

  if (is_last_ref)
    return;

  /* We just got a second ref, while calling g_weak_ref_get().
   *
   * Test that taking another weak ref in this situation works.
   */

  g_weak_ref_init (&weak2, object);
  g_assert_true (object == g_weak_ref_get (&weak2));
  g_object_unref (object);

  obj2 = g_object_new (G_TYPE_OBJECT, NULL);
  g_weak_ref_set (&weak2, obj2);
  g_object_unref (obj2);

  g_assert_null (g_weak_ref_get (&weak2));
}

static void
test_weak_ref_in_toggle_notify (void)
{
  GObject *obj;
  GWeakRef weak = { { GUINT_TO_POINTER (0xDEADBEEFU) } };

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_add_toggle_ref (obj, weak_ref_in_toggle_notify_toggle_cb, NULL);
  g_object_unref (obj);

  g_weak_ref_init (&weak, obj);

  /* We trigger a toggle notify via g_weak_ref_get(). */
  g_assert_true (g_weak_ref_get (&weak) == obj);

  g_object_remove_toggle_ref (obj, weak_ref_in_toggle_notify_toggle_cb, NULL);
  g_object_unref (obj);

  g_assert_null (g_weak_ref_get (&weak));
}

static void
test_weak_ref_many (void)
{
  const guint N = g_test_slow ()
                      ? G_MAXUINT16
                      : 211;
  const guint PRIME = 1048583;
  GObject *obj;
  GWeakRef *weak_refs;
  GWeakRef weak_ref1;
  guint j;
  guint n;
  guint i;

  obj = g_object_new (G_TYPE_OBJECT, NULL);

  weak_refs = g_new (GWeakRef, N);

  /* We register them in a somewhat juggled order. That's because below, we will clear them
   * again, and we don't want to always clear them in the same order as they were registered.
   * For that, we calculate the actual index by jumping around by adding a prime number. */
  j = (g_test_rand_int () % (N + 1));
  for (i = 0; i < N; i++)
    {
      j = (j + PRIME) % N;
      g_weak_ref_init (&weak_refs[j], obj);
    }

  if (N == G_MAXUINT16)
    {
      g_test_expect_message ("GLib-GObject", G_LOG_LEVEL_CRITICAL, "*Too many GWeakRef registered");
      g_weak_ref_init (&weak_ref1, obj);
      g_test_assert_expected_messages ();
      g_assert_null (g_weak_ref_get (&weak_ref1));
    }

  n = g_test_rand_int () % (N + 1u);
  for (i = 0; i < N; i++)
    g_weak_ref_set (&weak_refs[i], i < n ? NULL : obj);

  g_object_unref (obj);

  for (i = 0; i < N; i++)
    g_assert_null (g_weak_ref_get (&weak_refs[i]));

  /* The API would expect us to also call g_weak_ref_clear() on all references
   * to clean up.  In practice, they are already all NULL, so we don't need
   * that (it would have no effect, with the current implementation of
   * GWeakRef). */

  g_free (weak_refs);
}

/*****************************************************************************/

#define CONCURRENT_N_OBJS 5
#define CONCURRENT_N_THREADS 5
#define CONCURRENT_N_RACES 100

typedef struct
{
  int TEST_IDX;
  GObject *objs[CONCURRENT_N_OBJS];
  int thread_done[CONCURRENT_N_THREADS];
} ConcurrentData;

typedef struct
{
  const ConcurrentData *data;
  int idx;
  int race_count;
  GWeakRef *weak_ref;
  GRand *rnd;
} ConcurrentThreadData;

static gpointer
_test_weak_ref_concurrent_thread_cb (gpointer data)
{
  ConcurrentThreadData *thread_data = data;

  while (TRUE)
    {
      gboolean all_done;
      int i;
      int r;

      for (r = 0; r < 15; r++)
        {
          GObject *obj_allocated = NULL;
          GObject *obj;
          GObject *obj2;
          gboolean got_race;

          /* Choose a random object */
          obj = thread_data->data->objs[g_rand_int (thread_data->rnd) % CONCURRENT_N_OBJS];
          if (thread_data->data->TEST_IDX > 0 && (g_rand_int (thread_data->rnd) % 4 == 0))
            {
              /* With TEST_IDX>0 also randomly choose NULL or a newly created
               * object. */
              if (g_rand_boolean (thread_data->rnd))
                obj = NULL;
              else
                {
                  obj_allocated = g_object_new (G_TYPE_OBJECT, NULL);
                  obj = obj_allocated;
                }
            }

          g_assert (!obj || G_IS_OBJECT (obj));

          g_weak_ref_set (thread_data->weak_ref, obj);

          /* get the weak-ref. If there is no race, we expect to get the same
           * object back. */
          obj2 = g_weak_ref_get (thread_data->weak_ref);

          g_assert (!obj2 || G_IS_OBJECT (obj2));
          if (!obj2)
            {
              g_assert (thread_data->data->TEST_IDX > 0);
            }
          if (obj != obj2)
            {
              int cnt;

              cnt = 0;
              for (i = 0; i < CONCURRENT_N_OBJS; i++)
                {
                  if (obj2 == thread_data->data->objs[i])
                    cnt++;
                }
              if (!obj2)
                g_assert_cmpint (cnt, ==, 0);
              else if (obj2 && obj2 == obj_allocated)
                g_assert_cmpint (cnt, ==, 0);
              else if (thread_data->data->TEST_IDX > 0)
                g_assert_cmpint (cnt, <=, 1);
              else
                g_assert_cmpint (cnt, ==, 1);
              got_race = TRUE;
            }
          else
            got_race = FALSE;

          g_clear_object (&obj2);
          g_clear_object (&obj_allocated);

          if (got_race)
            {
              /* Each thread should see CONCURRENT_N_RACES before being done.
               * Count them. */
              if (g_atomic_int_get (&thread_data->race_count) > CONCURRENT_N_RACES)
                g_atomic_int_set (&thread_data->data->thread_done[thread_data->idx], 1);
              else
                g_atomic_int_add (&thread_data->race_count, 1);
            }
        }

      /* Each thread runs, until all threads saw the expected number of races. */
      all_done = TRUE;
      for (i = 0; i < CONCURRENT_N_THREADS; i++)
        {
          if (!g_atomic_int_get (&thread_data->data->thread_done[i]))
            {
              all_done = FALSE;
              break;
            }
        }
      if (all_done)
        return GINT_TO_POINTER (1);
    }
}

static void
test_weak_ref_concurrent (gconstpointer testdata)
{
  const int TEST_IDX = GPOINTER_TO_INT (testdata);
  GThread *threads[CONCURRENT_N_THREADS];
  int i;
  ConcurrentData data = {
    .TEST_IDX = TEST_IDX,
  };
  ConcurrentThreadData thread_data[CONCURRENT_N_THREADS];
  GWeakRef weak_ref = { 0 };

  /* Let several threads call g_weak_ref_set() & g_weak_ref_get() in a loop. */

  for (i = 0; i < CONCURRENT_N_OBJS; i++)
    data.objs[i] = g_object_new (G_TYPE_OBJECT, NULL);

  for (i = 0; i < CONCURRENT_N_THREADS; i++)
    {
      const guint32 rnd_seed[] = {
        g_test_rand_int (),
        g_test_rand_int (),
        g_test_rand_int (),
      };

      thread_data[i] = (ConcurrentThreadData){
        .idx = i,
        .data = &data,
        .weak_ref = &weak_ref,
        .rnd = g_rand_new_with_seed_array (rnd_seed, G_N_ELEMENTS (rnd_seed)),
      };
      threads[i] = g_thread_new ("test-weak-ref-concurrent", _test_weak_ref_concurrent_thread_cb, &thread_data[i]);
    }

  for (i = 0; i < CONCURRENT_N_THREADS; i++)
    {
      gpointer r;

      r = g_thread_join (g_steal_pointer (&threads[i]));
      g_assert_cmpint (GPOINTER_TO_INT (r), ==, 1);
    }

  for (i = 0; i < CONCURRENT_N_OBJS; i++)
    g_object_unref (g_steal_pointer (&data.objs[i]));
  for (i = 0; i < CONCURRENT_N_THREADS; i++)
    g_rand_free (g_steal_pointer (&thread_data[i].rnd));
}

/*****************************************************************************/

typedef struct
{
  gboolean should_be_last;
  gint count;
} Count;

static void
toggle_notify (gpointer  data,
               GObject  *obj,
               gboolean  is_last)
{
  Count *c = data;

  g_assert (is_last == c->should_be_last);

  if (is_last)
    g_assert_cmpint (g_atomic_int_get (&obj->ref_count), ==, 1);
  else
    g_assert_cmpint (g_atomic_int_get (&obj->ref_count), ==, 2);

  c->count++;
}

static void
test_toggle_ref (void)
{
  GObject *obj;
  Count c, c2;

  obj = g_object_new (G_TYPE_OBJECT, NULL);

  g_object_add_toggle_ref (obj, toggle_notify, &c);
  g_object_add_toggle_ref (obj, toggle_notify, &c2);

  c.should_be_last = c2.should_be_last = TRUE;
  c.count = c2.count = 0;

  g_object_unref (obj);

  g_assert_cmpint (c.count, ==, 0);
  g_assert_cmpint (c2.count, ==, 0);

  g_object_ref (obj);

  g_assert_cmpint (c.count, ==, 0);
  g_assert_cmpint (c2.count, ==, 0);

  g_object_remove_toggle_ref (obj, toggle_notify, &c2);

  g_object_unref (obj);

  g_assert_cmpint (c.count, ==, 1);

  c.should_be_last = FALSE;

  g_object_ref (obj);

  g_assert_cmpint (c.count, ==, 2);

  c.should_be_last = TRUE;

  g_object_unref (obj);

  g_assert_cmpint (c.count, ==, 3);

  g_object_remove_toggle_ref (obj, toggle_notify, &c);
}

G_DECLARE_FINAL_TYPE (DisposeReffingObject, dispose_reffing_object,
                      DISPOSE, REFFING_OBJECT, GObject)

typedef enum
{
  PROP_INT_PROP = 1,
  N_PROPS,
} DisposeReffingObjectProperty;

static GParamSpec *dispose_reffing_object_properties[N_PROPS] = {0};

struct _DisposeReffingObject
{
  GObject parent;

  GToggleNotify toggle_notify;
  Count actual;
  Count expected;
  unsigned disposing_refs;
  gboolean disposing_refs_all_normal;

  GCallback notify_handler;
  unsigned notify_called;

  int int_prop;

  GWeakRef *weak_ref;
};

G_DEFINE_TYPE (DisposeReffingObject, dispose_reffing_object, G_TYPE_OBJECT)

static void
on_object_notify (GObject    *object,
                  GParamSpec *pspec,
                  void       *data)
{
  DisposeReffingObject *obj = DISPOSE_REFFING_OBJECT (object);

  obj->notify_called++;
}

static void
dispose_reffing_object_dispose (GObject *object)
{
  DisposeReffingObject *obj = DISPOSE_REFFING_OBJECT (object);

  g_assert_cmpint (object->ref_count, ==, 1);
  g_assert_cmpint (obj->actual.count, ==, obj->expected.count);

  for (unsigned i = 0; i < obj->disposing_refs; ++i)
    {
      if (i == 0 && !obj->disposing_refs_all_normal)
        {
          g_object_add_toggle_ref (object, obj->toggle_notify, &obj->actual);
        }
      else
        {
          obj->actual.should_be_last = FALSE;
          g_object_ref (obj);
          g_assert_cmpint (obj->actual.count, ==, obj->expected.count);
        }

      obj->actual.should_be_last = TRUE;
    }

  G_OBJECT_CLASS (dispose_reffing_object_parent_class)->dispose (object);

  if (obj->notify_handler)
    {
      unsigned old_notify_called = obj->notify_called;

      g_assert_cmpuint (g_signal_handler_find (object, G_SIGNAL_MATCH_FUNC,
                        0, 0, NULL, obj->notify_handler, NULL), ==, 0);

      g_signal_connect (object, "notify", G_CALLBACK (obj->notify_handler), NULL);

      /* This would trigger a toggle notification, but is not something we may
       * want with https://gitlab.gnome.org/GNOME/glib/-/merge_requests/2377
       * so, we only test this in case we have more than one ref
       */
      if (obj->toggle_notify == toggle_notify)
        g_assert_cmpint (obj->disposing_refs, >, 1);

      g_object_notify (object, "int-prop");
      g_assert_cmpuint (obj->notify_called, ==, old_notify_called);
    }

  g_assert_cmpint (obj->actual.count, ==, obj->expected.count);
}

static void
dispose_reffing_object_init (DisposeReffingObject *connector)
{
}

static void
dispose_reffing_object_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
  DisposeReffingObject *obj = DISPOSE_REFFING_OBJECT (object);

  switch ((DisposeReffingObjectProperty) property_id)
    {
    case PROP_INT_PROP:
      obj->int_prop = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
dispose_reffing_object_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
  DisposeReffingObject *obj = DISPOSE_REFFING_OBJECT (object);

  switch ((DisposeReffingObjectProperty) property_id)
    {
    case PROP_INT_PROP:
      g_value_set_int (value, obj->int_prop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
dispose_reffing_object_class_init (DisposeReffingObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  dispose_reffing_object_properties[PROP_INT_PROP] =
      g_param_spec_int ("int-prop", "int-prop", "int-prop",
                        G_MININT, G_MAXINT,
                        0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  object_class->dispose = dispose_reffing_object_dispose;
  object_class->set_property = dispose_reffing_object_set_property;
  object_class->get_property = dispose_reffing_object_get_property;

  g_object_class_install_properties (object_class, N_PROPS,
                                     dispose_reffing_object_properties);
}

static void
test_toggle_ref_on_dispose (void)
{
  DisposeReffingObject *obj;
  gpointer disposed_checker = &obj;

  /* This tests wants to ensure that an object that gets re-referenced
   * (one or multiple times) during its dispose virtual function:
   *  - Notifies all the queued "notify" signal handlers
   *  - Notifies toggle notifications if any
   *  - It does not get finalized
   */

  obj = g_object_new (dispose_reffing_object_get_type (), NULL);
  obj->toggle_notify = toggle_notify;
  obj->notify_handler = G_CALLBACK (on_object_notify);
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  /* Convert to toggle notification */
  g_object_add_toggle_ref (G_OBJECT (obj), obj->toggle_notify, &obj->actual);
  g_assert_cmpint (obj->actual.count, ==, 0);

  obj->actual.should_be_last = TRUE;
  obj->notify_handler = G_CALLBACK (on_object_notify);
  g_object_unref (obj);
  g_assert_cmpint (obj->actual.count, ==, 1);
  g_assert_cmpuint (obj->notify_called, ==, 0);

  /* Remove the toggle reference, making it to dispose and resurrect again */
  obj->disposing_refs = 1;
  obj->expected.count = 1;
  obj->notify_handler = NULL; /* FIXME: enable it when !2377 is in */
  g_object_remove_toggle_ref (G_OBJECT (obj), obj->toggle_notify, NULL);
  g_assert_cmpint (obj->actual.count, ==, 2);
  g_assert_cmpuint (obj->notify_called, ==, 0);

  g_assert_null (disposed_checker);
  g_assert_cmpint (g_atomic_int_get (&G_OBJECT (obj)->ref_count), ==,
                   obj->disposing_refs);

  /* Object has been disposed, but is still alive, so add another weak pointer */
  disposed_checker = &obj;
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  /* Remove the toggle reference, making it to dispose and resurrect with
   * more references than before, so that no toggle notify is called
   */
  obj->disposing_refs = 3;
  obj->expected.count = 2;
  obj->notify_handler = G_CALLBACK (on_object_notify);
  g_object_remove_toggle_ref (G_OBJECT (obj), obj->toggle_notify, NULL);
  g_assert_cmpint (obj->actual.count, ==, 2);
  g_assert_cmpint (obj->notify_called, ==, 1);
  obj->expected.count = obj->actual.count;

  g_assert_null (disposed_checker);
  g_assert_cmpint (g_atomic_int_get (&G_OBJECT (obj)->ref_count), ==,
                   obj->disposing_refs);

  disposed_checker = &obj;
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  /* Now remove the first added reference */
  obj->disposing_refs = 0;
  g_object_unref (obj);
  g_assert_nonnull (disposed_checker);
  g_assert_cmpint (g_atomic_int_get (&G_OBJECT (obj)->ref_count), ==, 2);
  g_assert_cmpint (obj->actual.count, ==, 2);
  g_assert_cmpint (obj->notify_called, ==, 1);

  /* And the toggle one */
  obj->actual.should_be_last = TRUE;
  obj->notify_handler = NULL;
  g_object_remove_toggle_ref (G_OBJECT (obj), obj->toggle_notify, NULL);
  g_assert_nonnull (disposed_checker);
  g_assert_cmpint (g_atomic_int_get (&G_OBJECT (obj)->ref_count), ==, 1);
  g_assert_cmpint (obj->actual.count, ==, 2);
  obj->expected.count = obj->actual.count;

  g_clear_object (&obj);
  g_assert_null (disposed_checker);
}

static void
toggle_notify_counter (gpointer  data,
                       GObject  *obj,
                       gboolean  is_last)
{
  Count *c = data;
  c->count++;

  if (is_last)
    g_assert_cmpint (g_atomic_int_get (&obj->ref_count), ==, 1);
  else
    g_assert_cmpint (g_atomic_int_get (&obj->ref_count), ==, 2);
}

static void
on_object_notify_switch_to_normal_ref (GObject    *object,
                                       GParamSpec *pspec,
                                       void       *data)
{
  DisposeReffingObject *obj = DISPOSE_REFFING_OBJECT (object);

  obj->notify_called++;

  g_object_ref (object);
  g_object_remove_toggle_ref (object, obj->toggle_notify, NULL);
}

static void
on_object_notify_switch_to_toggle_ref (GObject    *object,
                                       GParamSpec *pspec,
                                       void       *data)
{
  DisposeReffingObject *obj = DISPOSE_REFFING_OBJECT (object);

  obj->notify_called++;

  g_object_add_toggle_ref (object, obj->toggle_notify, &obj->actual);
  g_object_unref (object);
}

static void
on_object_notify_add_ref (GObject    *object,
                          GParamSpec *pspec,
                          void       *data)
{
  DisposeReffingObject *obj = DISPOSE_REFFING_OBJECT (object);
  int old_toggle_cout = obj->actual.count;

  obj->notify_called++;

  g_object_ref (object);
  g_assert_cmpint (obj->actual.count, ==, old_toggle_cout);
}

static void
test_toggle_ref_and_notify_on_dispose (void)
{
  DisposeReffingObject *obj;
  gpointer disposed_checker = &obj;

  /* This tests wants to ensure that toggle signal emission during dispose
   * is properly working if the object is revitalized by adding new references.
   * It also wants to check that toggle notifications are not happening if a
   * notify handler is removing them at this phase.
   */

  obj = g_object_new (dispose_reffing_object_get_type (), NULL);
  obj->toggle_notify = toggle_notify_counter;
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  /* Convert to toggle notification */
  g_object_add_toggle_ref (G_OBJECT (obj), obj->toggle_notify, &obj->actual);
  g_assert_cmpint (obj->actual.count, ==, 0);

  obj->notify_handler = G_CALLBACK (on_object_notify);
  g_object_unref (obj);
  g_assert_cmpint (obj->actual.count, ==, 1);
  g_assert_cmpuint (obj->notify_called, ==, 0);

  disposed_checker = &obj;
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  /* Check that notification is triggered after being queued */
  obj->disposing_refs = 1;
  obj->expected.count = 1;
  obj->notify_handler = G_CALLBACK (on_object_notify);
  g_object_remove_toggle_ref (G_OBJECT (obj), obj->toggle_notify, NULL);
  g_assert_cmpint (obj->actual.count, ==, 2);
  g_assert_cmpuint (obj->notify_called, ==, 1);

  disposed_checker = &obj;
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  /* Check that notification is triggered after being queued, but no toggle
   * notification is happening if notify handler switches to normal reference
   */
  obj->disposing_refs = 1;
  obj->expected.count = 2;
  obj->notify_handler = G_CALLBACK (on_object_notify_switch_to_normal_ref);
  g_object_remove_toggle_ref (G_OBJECT (obj), obj->toggle_notify, NULL);
  g_assert_cmpint (obj->actual.count, ==, 2);
  g_assert_cmpuint (obj->notify_called, ==, 2);

  disposed_checker = &obj;
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  /* Check that notification is triggered after being queued, but that toggle
   * is happening if notify handler switched to toggle reference
   */
  obj->disposing_refs = 1;
  obj->disposing_refs_all_normal = TRUE;
  obj->expected.count = 2;
  obj->notify_handler = G_CALLBACK (on_object_notify_switch_to_toggle_ref);
  g_object_unref (obj);
  g_assert_cmpint (obj->actual.count, ==, 3);
  g_assert_cmpuint (obj->notify_called, ==, 3);

  disposed_checker = &obj;
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  /* Check that notification is triggered after being queued, but that toggle
   * is not happening if current refcount changed.
   */
  obj->disposing_refs = 1;
  obj->disposing_refs_all_normal = FALSE;
  obj->expected.count = 3;
  obj->notify_handler = G_CALLBACK (on_object_notify_add_ref);
  g_object_remove_toggle_ref (G_OBJECT (obj), obj->toggle_notify, NULL);
  g_assert_cmpint (obj->actual.count, ==, 3);
  g_assert_cmpuint (obj->notify_called, ==, 4);
  g_object_unref (obj);

  disposed_checker = &obj;
  g_object_add_weak_pointer (G_OBJECT (obj), &disposed_checker);

  obj->disposing_refs = 0;
  obj->expected.count = 4;
  g_clear_object (&obj);
  g_assert_null (disposed_checker);
}

static gboolean global_destroyed;
static gint global_value;

static void
data_destroy (gpointer data)
{
  g_assert_cmpint (GPOINTER_TO_INT (data), ==, global_value);

  global_destroyed = TRUE;
}

static void
test_object_qdata (void)
{
  GObject *obj;
  gpointer v;
  GQuark quark;

  obj = g_object_new (G_TYPE_OBJECT, NULL);

  global_value = 1;
  global_destroyed = FALSE;
  g_object_set_data_full (obj, "test", GINT_TO_POINTER (1), data_destroy);
  v = g_object_get_data (obj, "test");
  g_assert_cmpint (GPOINTER_TO_INT (v), ==, 1);
  g_object_set_data_full (obj, "test", GINT_TO_POINTER (2), data_destroy);
  g_assert (global_destroyed);
  global_value = 2;
  global_destroyed = FALSE;
  v = g_object_steal_data (obj, "test");
  g_assert_cmpint (GPOINTER_TO_INT (v), ==, 2);
  g_assert (!global_destroyed);

  global_value = 1;
  global_destroyed = FALSE;
  quark = g_quark_from_string ("test");
  g_object_set_qdata_full (obj, quark, GINT_TO_POINTER (1), data_destroy);
  v = g_object_get_qdata (obj, quark);
  g_assert_cmpint (GPOINTER_TO_INT (v), ==, 1);
  g_object_set_qdata_full (obj, quark, GINT_TO_POINTER (2), data_destroy);
  g_assert (global_destroyed);
  global_value = 2;
  global_destroyed = FALSE;
  v = g_object_steal_qdata (obj, quark);
  g_assert_cmpint (GPOINTER_TO_INT (v), ==, 2);
  g_assert (!global_destroyed);

  g_object_set_qdata_full (obj, quark, GINT_TO_POINTER (3), data_destroy);
  global_value = 3;
  global_destroyed = FALSE;
  g_object_unref (obj);

  g_assert (global_destroyed);
}

typedef struct {
  const gchar *value;
  gint refcount;
} Value;

static gpointer
ref_value (gpointer value, gpointer user_data)
{
  Value *v = value;
  Value **old_value_p = user_data;

  if (old_value_p)
    *old_value_p = v;

  if (v)
    v->refcount += 1;

  return value;
}

static void
unref_value (gpointer value)
{
  Value *v = value;

  v->refcount -= 1;
  if (v->refcount == 0)
    g_free (value);
}

static
Value *
new_value (const gchar *s)
{
  Value *v;

  v = g_new (Value, 1);
  v->value = s;
  v->refcount = 1;

  return v;
}

static void
test_object_qdata2 (void)
{
  GObject *obj;
  Value *v, *v1, *v2, *v3, *old_val;
  GDestroyNotify old_destroy;
  gboolean res;

  obj = g_object_new (G_TYPE_OBJECT, NULL);

  v1 = new_value ("bla");

  g_object_set_data_full (obj, "test", v1, unref_value);

  v = g_object_get_data (obj, "test");
  g_assert_cmpstr (v->value, ==, "bla");
  g_assert_cmpint (v->refcount, ==, 1);

  v = g_object_dup_data (obj, "test", ref_value, &old_val);
  g_assert (old_val == v1);
  g_assert_cmpstr (v->value, ==, "bla");
  g_assert_cmpint (v->refcount, ==, 2);
  unref_value (v);

  v = g_object_dup_data (obj, "nono", ref_value, &old_val);
  g_assert (old_val == NULL);
  g_assert (v == NULL);

  v2 = new_value ("not");

  res = g_object_replace_data (obj, "test", v1, v2, unref_value, &old_destroy);
  g_assert (res == TRUE);
  g_assert (old_destroy == unref_value);
  g_assert_cmpstr (v1->value, ==, "bla");
  g_assert_cmpint (v1->refcount, ==, 1);

  v = g_object_get_data (obj, "test");
  g_assert_cmpstr (v->value, ==, "not");
  g_assert_cmpint (v->refcount, ==, 1);

  v3 = new_value ("xyz");
  res = g_object_replace_data (obj, "test", v1, v3, unref_value, &old_destroy);
  g_assert (res == FALSE);
  g_assert_cmpstr (v2->value, ==, "not");
  g_assert_cmpint (v2->refcount, ==, 1);

  unref_value (v1);

  res = g_object_replace_data (obj, "test", NULL, v3, unref_value, &old_destroy);
  g_assert (res == FALSE);
  g_assert_cmpstr (v2->value, ==, "not");
  g_assert_cmpint (v2->refcount, ==, 1);

  res = g_object_replace_data (obj, "test", v2, NULL, unref_value, &old_destroy);
  g_assert (res == TRUE);
  g_assert (old_destroy == unref_value);
  g_assert_cmpstr (v2->value, ==, "not");
  g_assert_cmpint (v2->refcount, ==, 1);

  unref_value (v2);

  v = g_object_get_data (obj, "test");
  g_assert (v == NULL);

  res = g_object_replace_data (obj, "test", NULL, v3, unref_value, &old_destroy);
  g_assert (res == TRUE);

  v = g_object_get_data (obj, "test");
  g_assert (v == v3);

  ref_value (v3, NULL);
  g_assert_cmpint (v3->refcount, ==, 2);
  g_object_unref (obj);
  g_assert_cmpint (v3->refcount, ==, 1);
  unref_value (v3);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_setenv ("G_ENABLE_DIAGNOSTIC", "1", TRUE);

  g_test_add_func ("/type/fundamentals", test_fundamentals);
  g_test_add_func ("/type/qdata", test_type_qdata);
  g_test_add_func ("/type/query", test_type_query);
  g_test_add_func ("/type/class-private", test_class_private);
  g_test_add_func ("/object/clear", test_clear);
  g_test_add_func ("/object/clear-function", test_clear_function);
  g_test_add_func ("/object/set", test_set);
  g_test_add_func ("/object/set-function", test_set_function);
  g_test_add_func ("/object/set/derived-type", test_set_derived_type);
  g_test_add_func ("/object/value", test_object_value);
  g_test_add_func ("/object/initially-unowned", test_initially_unowned);
  g_test_add_func ("/object/weak-pointer", test_weak_pointer);
  g_test_add_func ("/object/weak-pointer/clear", test_weak_pointer_clear);
  g_test_add_func ("/object/weak-pointer/clear-function", test_weak_pointer_clear_function);
  g_test_add_func ("/object/weak-pointer/set", test_weak_pointer_set);
  g_test_add_func ("/object/weak-pointer/set-function", test_weak_pointer_set_function);
  g_test_add_func ("/object/weak-ref", test_weak_ref);
  g_test_add_func ("/object/weak-ref/on-dispose", test_weak_ref_on_dispose);
  g_test_add_func ("/object/weak-ref/on-run-dispose", test_weak_ref_on_run_dispose);
  g_test_add_func ("/object/weak-ref/on-toggle-notify", test_weak_ref_on_toggle_notify);
  g_test_add_func ("/object/weak-ref/in-toggle-notify", test_weak_ref_in_toggle_notify);
  g_test_add_func ("/object/weak-ref/many", test_weak_ref_many);
  g_test_add_data_func ("/object/weak-ref/concurrent/0", GINT_TO_POINTER (0), test_weak_ref_concurrent);
  g_test_add_data_func ("/object/weak-ref/concurrent/1", GINT_TO_POINTER (1), test_weak_ref_concurrent);
  g_test_add_func ("/object/toggle-ref", test_toggle_ref);
  g_test_add_func ("/object/toggle-ref/ref-on-dispose", test_toggle_ref_on_dispose);
  g_test_add_func ("/object/toggle-ref/ref-and-notify-on-dispose", test_toggle_ref_and_notify_on_dispose);
  g_test_add_func ("/object/qdata", test_object_qdata);
  g_test_add_func ("/object/qdata2", test_object_qdata2);

  return g_test_run ();
}
