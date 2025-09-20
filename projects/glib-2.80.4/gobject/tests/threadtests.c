/* GLib testing framework examples and tests
 * Copyright (C) 2008 Imendio AB
 * Authors: Tim Janik
 *
 * SPDX-License-Identifier: LicenseRef-old-glib-tests
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#ifndef GLIB_DISABLE_DEPRECATION_WARNINGS
#define GLIB_DISABLE_DEPRECATION_WARNINGS
#endif

#include <glib.h>
#include <glib-object.h>

static int mtsafe_call_counter = 0; /* multi thread safe call counter, must be accessed atomically */
static int unsafe_call_counter = 0; /* single-threaded call counter */
static GCond sync_cond;
static GMutex sync_mutex;

#define NUM_COUNTER_INCREMENTS 100000

static void
call_counter_init (gpointer tclass)
{
  int i;
  for (i = 0; i < NUM_COUNTER_INCREMENTS; i++)
    {
      int saved_unsafe_call_counter = unsafe_call_counter;
      g_atomic_int_add (&mtsafe_call_counter, 1); /* real call count update */
      g_thread_yield(); /* let concurrent threads corrupt the unsafe_call_counter state */
      unsafe_call_counter = 1 + saved_unsafe_call_counter; /* non-atomic counter update */
    }
}

static void interface_per_class_init (void) { call_counter_init (NULL); }

/* define 3 test interfaces */
typedef GTypeInterface MyFace0Interface;
static GType my_face0_get_type (void);
G_DEFINE_INTERFACE (MyFace0, my_face0, G_TYPE_OBJECT)
static void my_face0_default_init (MyFace0Interface *iface) { call_counter_init (iface); }
typedef GTypeInterface MyFace1Interface;
static GType my_face1_get_type (void);
G_DEFINE_INTERFACE (MyFace1, my_face1, G_TYPE_OBJECT)
static void my_face1_default_init (MyFace1Interface *iface) { call_counter_init (iface); }

/* define 3 test objects, adding interfaces 0 & 1, and adding interface 2 after class initialization */
typedef GObject         MyTester0;
typedef GObjectClass    MyTester0Class;
static GType my_tester0_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyTester0, my_tester0, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (my_face0_get_type(), interface_per_class_init)
                         G_IMPLEMENT_INTERFACE (my_face1_get_type(), interface_per_class_init))
static void my_tester0_init (MyTester0*t) {}
static void my_tester0_class_init (MyTester0Class*c) { call_counter_init (c); }
typedef GObject         MyTester1;
typedef GObjectClass    MyTester1Class;

/* Disabled for now (see https://bugzilla.gnome.org/show_bug.cgi?id=687659) */
#if 0
typedef GTypeInterface MyFace2Interface;
static GType my_face2_get_type (void);
G_DEFINE_INTERFACE (MyFace2, my_face2, G_TYPE_OBJECT)
static void my_face2_default_init (MyFace2Interface *iface) { call_counter_init (iface); }

static GType my_tester1_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyTester1, my_tester1, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (my_face0_get_type(), interface_per_class_init)
                         G_IMPLEMENT_INTERFACE (my_face1_get_type(), interface_per_class_init))
static void my_tester1_init (MyTester1*t) {}
static void my_tester1_class_init (MyTester1Class*c) { call_counter_init (c); }
typedef GObject         MyTester2;
typedef GObjectClass    MyTester2Class;
static GType my_tester2_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyTester2, my_tester2, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (my_face0_get_type(), interface_per_class_init)
                         G_IMPLEMENT_INTERFACE (my_face1_get_type(), interface_per_class_init))
static void my_tester2_init (MyTester2*t) {}
static void my_tester2_class_init (MyTester2Class*c) { call_counter_init (c); }

static gpointer
tester_init_thread (gpointer data)
{
  const GInterfaceInfo face2_interface_info = { (GInterfaceInitFunc) interface_per_class_init, NULL, NULL };
  gpointer klass;
  /* first, synchronize with other threads,
   * then run interface and class initializers,
   * using unsafe_call_counter concurrently
   */
  g_mutex_lock (&sync_mutex);
  g_mutex_unlock (&sync_mutex);
  /* test default interface initialization for face0 */
  g_type_default_interface_unref (g_type_default_interface_ref (my_face0_get_type()));
  /* test class initialization, face0 per-class initializer, face1 default and per-class initializer */
  klass = g_type_class_ref ((GType) data);
  /* test face2 default and per-class initializer, after class_init */
  g_type_add_interface_static (G_TYPE_FROM_CLASS (klass), my_face2_get_type(), &face2_interface_info);
  /* cleanups */
  g_type_class_unref (klass);
  return NULL;
}

static void
test_threaded_class_init (void)
{
  GThread *t1, *t2, *t3;

  /* pause newly created threads */
  g_mutex_lock (&sync_mutex);

  /* create threads */
  t1 = g_thread_create (tester_init_thread, (gpointer) my_tester0_get_type(), TRUE, NULL);
  t2 = g_thread_create (tester_init_thread, (gpointer) my_tester1_get_type(), TRUE, NULL);
  t3 = g_thread_create (tester_init_thread, (gpointer) my_tester2_get_type(), TRUE, NULL);

  /* execute threads */
  g_mutex_unlock (&sync_mutex);
  while (g_atomic_int_get (&mtsafe_call_counter) < (3 + 3 + 3 * 3) * NUM_COUNTER_INCREMENTS)
    {
      if (g_test_verbose())
        g_printerr ("Initializers counted: %u\n", g_atomic_int_get (&mtsafe_call_counter));
      g_usleep (50 * 1000); /* wait for threads to complete */
    }
  if (g_test_verbose())
    g_printerr ("Total initializers: %u\n", g_atomic_int_get (&mtsafe_call_counter));
  /* ensure non-corrupted counter updates */
  g_assert_cmpint (g_atomic_int_get (&mtsafe_call_counter), ==, unsafe_call_counter);

  g_thread_join (t1);
  g_thread_join (t2);
  g_thread_join (t3);
}
#endif

typedef struct {
  GObject parent;
  char   *name;
} PropTester;
typedef GObjectClass    PropTesterClass;
static GType prop_tester_get_type (void);
G_DEFINE_TYPE (PropTester, prop_tester, G_TYPE_OBJECT)
#define PROP_NAME 1
static void
prop_tester_init (PropTester* t)
{
  if (t->name == NULL)
    { } /* needs unit test framework initialization: g_test_bug ("race initializing properties"); */
}
static void
prop_tester_set_property (GObject        *object,
                          guint           property_id,
                          const GValue   *value,
                          GParamSpec     *pspec)
{}
static void
prop_tester_class_init (PropTesterClass *c)
{
  int i;
  GParamSpec *param;
  GObjectClass *gobject_class = G_OBJECT_CLASS (c);

  gobject_class->set_property = prop_tester_set_property; /* silence GObject checks */

  g_mutex_lock (&sync_mutex);
  g_cond_signal (&sync_cond);
  g_mutex_unlock (&sync_mutex);

  for (i = 0; i < 100; i++) /* wait a bit. */
    g_thread_yield();

  call_counter_init (c);
  param = g_param_spec_string ("name", "name_i18n",
			       "yet-more-wasteful-i18n",
			       NULL,
			       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
			       G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB |
			       G_PARAM_STATIC_NICK);
  g_object_class_install_property (gobject_class, PROP_NAME, param);
}

static gpointer
object_create (gpointer data)
{
  GObject *obj = g_object_new (prop_tester_get_type(), "name", "fish", NULL);
  g_object_unref (obj);
  return NULL;
}

static void
test_threaded_object_init (void)
{
  GThread *creator;
  g_mutex_lock (&sync_mutex);

  creator = g_thread_create (object_create, NULL, TRUE, NULL);
  /* really provoke the race */
  g_cond_wait (&sync_cond, &sync_mutex);

  object_create (NULL);
  g_mutex_unlock (&sync_mutex);

  g_thread_join (creator);
}

typedef struct {
    MyTester0 *strong;
    guint unref_delay;
} UnrefInThreadData;

static gpointer
unref_in_thread (gpointer p)
{
  UnrefInThreadData *data = p;

  g_usleep (data->unref_delay);
  g_object_unref (data->strong);

  return NULL;
}

/* undefine to see this test fail without GWeakRef */
#define HAVE_G_WEAK_REF

#define SLEEP_MIN_USEC 1
#define SLEEP_MAX_USEC 10

static void
test_threaded_weak_ref (void)
{
  guint i;
  guint get_wins = 0, unref_wins = 0;
  guint n;

  if (g_test_thorough ())
    n = NUM_COUNTER_INCREMENTS;
  else
    n = NUM_COUNTER_INCREMENTS / 20;

#ifdef G_OS_WIN32
  /* On Windows usleep has millisecond resolution and gets rounded up
   * leading to the test running for a long time. */
  n /= 10;
#endif

  for (i = 0; i < n; i++)
    {
      UnrefInThreadData data;
#ifdef HAVE_G_WEAK_REF
      /* GWeakRef<MyTester0> in C++ terms */
      GWeakRef weak;
#else
      gpointer weak;
#endif
      MyTester0 *strengthened;
      guint get_delay;
      GThread *thread;
      GError *error = NULL;

      if (g_test_verbose () && (i % (n/20)) == 0)
        g_printerr ("%u%%\n", ((i * 100) / n));

      /* Have an object and a weak ref to it */
      data.strong = g_object_new (my_tester0_get_type (), NULL);

#ifdef HAVE_G_WEAK_REF
      g_weak_ref_init (&weak, data.strong);
#else
      weak = data.strong;
      g_object_add_weak_pointer ((GObject *) weak, &weak);
#endif

      /* Delay for a random time on each side of the race, to perturb the
       * timing. Ideally, we want each side to win half the races; on
       * smcv's laptop, these timings are about right.
       */
      data.unref_delay = g_random_int_range (SLEEP_MIN_USEC / 2, SLEEP_MAX_USEC / 2);
      get_delay = g_random_int_range (SLEEP_MIN_USEC, SLEEP_MAX_USEC);

      /* One half of the race is to unref the shared object */
      thread = g_thread_create (unref_in_thread, &data, TRUE, &error);
      g_assert_no_error (error);

      /* The other half of the race is to get the object from the "global
       * singleton"
       */
      g_usleep (get_delay);

#ifdef HAVE_G_WEAK_REF
      strengthened = g_weak_ref_get (&weak);
#else
      /* Spot the unsafe pointer access! In GDBusConnection this is rather
       * better-hidden, but ends up with essentially the same thing, albeit
       * cleared in dispose() rather than by a traditional weak pointer
       */
      strengthened = weak;

      if (strengthened != NULL)
        g_object_ref (strengthened);
#endif

      if (strengthened != NULL)
        g_assert (G_IS_OBJECT (strengthened));

      /* Wait for the thread to run */
      g_thread_join (thread);

      if (strengthened != NULL)
        {
          get_wins++;
          g_assert (G_IS_OBJECT (strengthened));
          g_object_unref (strengthened);
        }
      else
        {
          unref_wins++;
        }

#ifdef HAVE_G_WEAK_REF
      g_weak_ref_clear (&weak);
#else
      if (weak != NULL)
        g_object_remove_weak_pointer (weak, &weak);
#endif
    }

  if (g_test_verbose ())
    g_printerr ("Race won by get %u times, unref %u times\n",
             get_wins, unref_wins);
}

typedef struct
{
  GObject *object;
  GWeakRef *weak;
  gint started; /* (atomic) */
  gint finished; /* (atomic) */
  gint disposing; /* (atomic) */
} ThreadedWeakRefData;

static void
on_weak_ref_disposed (gpointer data,
                      GObject *gobj)
{
  ThreadedWeakRefData *thread_data = data;

  /* Wait until the thread has started */
  while (!g_atomic_int_get (&thread_data->started))
    continue;

  g_atomic_int_set (&thread_data->disposing, 1);

  /* Wait for the thread to act, so that the object is still valid */
  while (!g_atomic_int_get (&thread_data->finished))
    continue;

  g_atomic_int_set (&thread_data->disposing, 0);
}

static gpointer
on_other_thread_weak_ref (gpointer user_data)
{
  ThreadedWeakRefData *thread_data = user_data;
  GObject *object = thread_data->object;

  g_atomic_int_set (&thread_data->started, 1);

  /* Ensure we've started disposal */
  while (!g_atomic_int_get (&thread_data->disposing))
    continue;

  g_object_ref (object);
  g_weak_ref_set (thread_data->weak, object);
  g_object_unref (object);

  g_assert_cmpint (thread_data->disposing, ==, 1);
  g_atomic_int_set (&thread_data->finished, 1);

  return NULL;
}

static void
test_threaded_weak_ref_finalization (void)
{
  GObject *obj = g_object_new (G_TYPE_OBJECT, NULL);
  GWeakRef weak = { { GUINT_TO_POINTER (0xDEADBEEFU) } };
  ThreadedWeakRefData thread_data = {
    .object = obj, .weak = &weak, .started = 0, .finished = 0
  };

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/issues/2390");
  g_test_summary ("Test that a weak ref added by another thread during dispose "
                  "of a GObject is cleared during finalisation. "
                  "Use on_weak_ref_disposed() to synchronize the other thread "
                  "with the dispose vfunc.");

  g_weak_ref_init (&weak, NULL);
  g_object_weak_ref (obj, on_weak_ref_disposed, &thread_data);

  g_assert_cmpint (obj->ref_count, ==, 1);
  g_thread_unref (g_thread_new ("on_other_thread",
                                on_other_thread_weak_ref,
                                &thread_data));
  g_object_unref (obj);

  /* This is what this test is about: at this point the weak reference
   * should have been unset (and not point to a dead object either). */
  g_assert_null (g_weak_ref_get (&weak));
}

typedef struct
{
  GObject *object;
  int done;    /* (atomic) */
  int toggles; /* (atomic) */
} ToggleNotifyThreadData;

static gpointer
on_reffer_thread (gpointer user_data)
{
  ToggleNotifyThreadData *thread_data = user_data;

  while (!g_atomic_int_get (&thread_data->done))
    {
      g_object_ref (thread_data->object);
      g_object_unref (thread_data->object);
    }

  return NULL;
}

static void
on_toggle_notify (gpointer data,
                  GObject *object,
                  gboolean is_last_ref)
{
  /* Anything could be put here, but we don't care for this test.
   * Actually having this empty made the bug to happen more frequently (being
   * timing related).
   */
}

static gpointer
on_toggler_thread (gpointer user_data)
{
  ToggleNotifyThreadData *thread_data = user_data;

  while (!g_atomic_int_get (&thread_data->done))
    {
      g_object_ref (thread_data->object);
      g_object_remove_toggle_ref (thread_data->object, on_toggle_notify, thread_data);
      g_object_add_toggle_ref (thread_data->object, on_toggle_notify, thread_data);
      g_object_unref (thread_data->object);
      g_atomic_int_add (&thread_data->toggles, 1);
    }

  return NULL;
}

static void
test_threaded_toggle_notify (void)
{
  GObject *object = g_object_new (G_TYPE_OBJECT, NULL);
  ToggleNotifyThreadData data = { object, FALSE, 0 };
  GThread *threads[3];
  gsize i;
  const int n_iterations = g_test_thorough () ? 1000000 : 100000;

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/issues/2394");
  g_test_summary ("Test that toggle reference notifications can be changed "
                  "safely from another (the main) thread without causing the "
                  "notifying thread to abort");

  g_object_add_toggle_ref (object, on_toggle_notify, &data);
  g_object_unref (object);

  g_assert_cmpint (object->ref_count, ==, 1);
  threads[0] = g_thread_new ("on_reffer_thread", on_reffer_thread, &data);
  threads[1] = g_thread_new ("on_another_reffer_thread", on_reffer_thread, &data);
  threads[2] = g_thread_new ("on_main_toggler_thread", on_toggler_thread, &data);

  /* We need to wait here for the threads to run for a bit in order to make the
   * race to happen, so we wait for an high number of toggle changes to be met
   * so that we can be consistent on each platform.
   */
  while (g_atomic_int_get (&data.toggles) < n_iterations)
    ;
  g_atomic_int_set (&data.done, TRUE);

  for (i = 0; i < G_N_ELEMENTS (threads); i++)
    g_thread_join (threads[i]);

  g_assert_cmpint (object->ref_count, ==, 1);
  g_clear_object (&object);
}

static void
test_threaded_g_pointer_bit_unlock_and_set (void)
{
  GObject *obj;
  gpointer plock;
  gpointer ptr;
  gpointer ptr2;
  gpointer mangled_obj;

#if defined(__GNUC__)
  /* We should have at least one bit we can use safely for bit-locking */
  G_STATIC_ASSERT (__alignof (GObject) > 1);
#endif

  obj = g_object_new (G_TYPE_OBJECT, NULL);

  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 0, 0, NULL) == obj);
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 0, 0x2, obj) == obj);
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 1, 0, NULL) != obj);

  mangled_obj = obj;
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 0, 0x2, mangled_obj) == obj);
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 0, 0x3, mangled_obj) == obj);
  g_atomic_pointer_and (&mangled_obj, ~((gsize) 0x7));
  g_atomic_pointer_or (&mangled_obj, 0x2);
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 0, 0x2, mangled_obj) != obj);
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 0, 0x2, mangled_obj) == (gpointer) (((guintptr) obj) | ((guintptr) mangled_obj)));
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 0, 0x3, mangled_obj) == (gpointer) (((guintptr) obj) | ((guintptr) mangled_obj)));
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, TRUE, 0x3, mangled_obj) == (gpointer) (((guintptr) obj) | ((guintptr) mangled_obj) | ((guintptr) 1)));
  g_atomic_pointer_and (&mangled_obj, ~((gsize) 0x2));
  g_assert_true (g_pointer_bit_lock_mask_ptr (obj, 0, 0, 0x2, mangled_obj) == obj);
  g_atomic_pointer_or (&mangled_obj, 0x2);

  plock = obj;
  g_pointer_bit_lock (&plock, 0);
  g_assert_true (plock != obj);
  g_pointer_bit_unlock_and_set (&plock, 0, obj, 0);
  g_assert_true (plock == obj);

  plock = obj;
  g_pointer_bit_lock_and_get (&plock, 0, &ptr2);
  g_assert_true (ptr2 == plock);
  g_assert_true (plock != obj);
  g_atomic_pointer_set (&plock, mangled_obj);
  g_pointer_bit_unlock_and_set (&plock, 0, obj, 0);
  g_assert_true (plock == obj);

  plock = obj;
  g_pointer_bit_lock_and_get (&plock, 0, NULL);
  g_assert_true (plock != obj);
  g_atomic_pointer_set (&plock, mangled_obj);
  g_pointer_bit_unlock_and_set (&plock, 0, obj, 0x7);
  g_assert_true (plock != obj);
  g_assert_true (plock == (gpointer) (((guintptr) obj) | ((guintptr) mangled_obj)));

  plock = NULL;
  g_pointer_bit_lock (&plock, 0);
  g_assert_true (plock != NULL);
  g_pointer_bit_unlock_and_set (&plock, 0, NULL, 0);
  g_assert_true (plock == NULL);

  ptr = ((char *) obj) + 1;
  plock = obj;
  g_pointer_bit_lock (&plock, 0);
  g_assert_true (plock == ptr);
  g_test_expect_message ("GLib", G_LOG_LEVEL_CRITICAL,
                         "*assertion 'ptr == pointer_bit_lock_mask_ptr (ptr, lock_bit, FALSE, 0, NULL)' failed*");
  g_pointer_bit_unlock_and_set (&plock, 0, ptr, 0);
  g_test_assert_expected_messages ();
  g_assert_true (plock != ptr);
  g_assert_true (plock == obj);

  g_object_unref (obj);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* g_test_add_func ("/GObject/threaded-class-init", test_threaded_class_init); */
  g_test_add_func ("/GObject/threaded-object-init", test_threaded_object_init);
  g_test_add_func ("/GObject/threaded-weak-ref", test_threaded_weak_ref);
  g_test_add_func ("/GObject/threaded-weak-ref/on-finalization",
                   test_threaded_weak_ref_finalization);
  g_test_add_func ("/GObject/threaded-toggle-notify",
                   test_threaded_toggle_notify);
  g_test_add_func ("/GObject/threaded-g-pointer-bit-unlock-and-set",
                   test_threaded_g_pointer_bit_unlock_and_set);

  return g_test_run();
}
