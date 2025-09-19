/* GObject - GLib Type, Object, Parameter and Signal Library
 *
 * Copyright (C) 2015-2022 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Garrett Regier <garrettregier@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib-object.h>

G_DECLARE_FINAL_TYPE (SignalTarget, signal_target, TEST, SIGNAL_TARGET, GObject)

struct _SignalTarget
{
  GObject parent_instance;
};

G_DEFINE_TYPE (SignalTarget, signal_target, G_TYPE_OBJECT)

static G_DEFINE_QUARK (detail, signal_detail);

enum {
  THE_SIGNAL,
  NEVER_EMITTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
signal_target_class_init (SignalTargetClass *klass)
{
  signals[THE_SIGNAL] =
      g_signal_new ("the-signal",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                    0,
                    NULL, NULL, NULL,
                    G_TYPE_NONE,
                    1,
                    G_TYPE_OBJECT);

  signals[NEVER_EMITTED] =
      g_signal_new ("never-emitted",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL, NULL,
                    G_TYPE_NONE,
                    1,
                    G_TYPE_OBJECT);
}

static void
signal_target_init (SignalTarget *self)
{
}

static gint global_signal_calls;
static gint global_weak_notify_called;

static void
connect_before_cb (SignalTarget *target,
                   GSignalGroup *group,
                   gint         *signal_calls)
{
  SignalTarget *readback;

  g_assert_true (TEST_IS_SIGNAL_TARGET (target));
  g_assert_true (G_IS_SIGNAL_GROUP (group));
  g_assert_nonnull (signal_calls);
  g_assert_true (signal_calls == &global_signal_calls);

  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target);
  g_object_unref (readback);

  *signal_calls += 1;
}

static void
connect_after_cb (SignalTarget *target,
                  GSignalGroup *group,
                  gint         *signal_calls)
{
  SignalTarget *readback;

  g_assert_true (TEST_IS_SIGNAL_TARGET (target));
  g_assert_true (G_IS_SIGNAL_GROUP (group));
  g_assert_nonnull (signal_calls);
  g_assert_true (signal_calls == &global_signal_calls);

  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target);
  g_object_unref (readback);

  g_assert_cmpint (*signal_calls, ==, 5);
  *signal_calls += 1;
}

static void
connect_swapped_cb (gint         *signal_calls,
                    GSignalGroup *group,
                    SignalTarget *target)
{
  SignalTarget *readback;

  g_assert_true (signal_calls != NULL);
  g_assert_true (signal_calls == &global_signal_calls);
  g_assert_true (G_IS_SIGNAL_GROUP (group));
  g_assert_true (TEST_IS_SIGNAL_TARGET (target));

  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target);
  g_object_unref (readback);

  *signal_calls += 1;
}

static void
connect_object_cb (SignalTarget *target,
                   GSignalGroup *group,
                   GObject      *object)
{
  SignalTarget *readback;
  gint *signal_calls;

  g_assert_true (TEST_IS_SIGNAL_TARGET (target));
  g_assert_true (G_IS_SIGNAL_GROUP (group));
  g_assert_true (G_IS_OBJECT (object));

  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target);
  g_object_unref (readback);

  signal_calls = g_object_get_data (object, "signal-calls");
  g_assert_nonnull (signal_calls);
  g_assert_true (signal_calls == &global_signal_calls);

  *signal_calls += 1;
}

static void
connect_bad_detail_cb (SignalTarget *target,
                       GSignalGroup *group,
                       GObject      *object)
{
  g_error ("This detailed signal is never emitted!");
}

static void
connect_never_emitted_cb (SignalTarget *target,
                          gboolean     *weak_notify_called)
{
  g_error ("This signal is never emitted!");
}

static void
connect_data_notify_cb (gboolean *weak_notify_called,
                        GClosure *closure)
{
  g_assert_nonnull (weak_notify_called);
  g_assert_true (weak_notify_called == &global_weak_notify_called);
  g_assert_nonnull (closure);

  g_assert_false (*weak_notify_called);
  *weak_notify_called = TRUE;
}

static void
connect_data_weak_notify_cb (gboolean     *weak_notify_called,
                             GSignalGroup *group)
{
  g_assert_nonnull (weak_notify_called);
  g_assert_true (weak_notify_called == &global_weak_notify_called);
  g_assert_true (G_IS_SIGNAL_GROUP (group));

  g_assert_true (*weak_notify_called);
}

static void
connect_all_signals (GSignalGroup *group)
{
  GObject  *object;
  GClosure *closure;

  /* Check that these are called in the right order */
  g_signal_group_connect (group,
                          "the-signal",
                          G_CALLBACK (connect_before_cb),
                          &global_signal_calls);
  g_signal_group_connect_after (group,
                                "the-signal",
                                G_CALLBACK (connect_after_cb),
                                &global_signal_calls);

  /* Check that this is called with the arguments swapped */
  g_signal_group_connect_swapped (group,
                                  "the-signal",
                                  G_CALLBACK (connect_swapped_cb),
                                  &global_signal_calls);

  /* Check that this is called with the arguments swapped */
  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_data (object, "signal-calls", &global_signal_calls);
  g_signal_group_connect_object (group,
                                 "the-signal",
                                 G_CALLBACK (connect_object_cb),
                                 object,
                                 0);
  g_object_weak_ref (G_OBJECT (group),
                     (GWeakNotify)g_object_unref,
                     object);

  /* Check that a detailed signal is handled correctly */
  g_signal_group_connect (group,
                          "the-signal::detail",
                          G_CALLBACK (connect_before_cb),
                          &global_signal_calls);
  g_signal_group_connect (group,
                          "the-signal::bad-detail",
                          G_CALLBACK (connect_bad_detail_cb),
                          NULL);

  /* Check that the notify is called correctly */
  global_weak_notify_called = FALSE;
  g_signal_group_connect_data (group,
                               "never-emitted",
                               G_CALLBACK (connect_never_emitted_cb),
                               &global_weak_notify_called,
                               (GClosureNotify)connect_data_notify_cb,
                               0);
  g_object_weak_ref (G_OBJECT (group),
                     (GWeakNotify)connect_data_weak_notify_cb,
                     &global_weak_notify_called);


  /* Check that this can be called as a GClosure */
  closure = g_cclosure_new (G_CALLBACK (connect_before_cb),
                            &global_signal_calls,
                            NULL);
  g_signal_group_connect_closure (group, "the-signal", closure, FALSE);

  /* Check that invalidated GClosures don't get called */
  closure = g_cclosure_new (G_CALLBACK (connect_before_cb),
                            &global_signal_calls,
                            NULL);
  g_closure_invalidate (closure);
  g_signal_group_connect_closure (group, "the-signal", closure, FALSE);
}

static void
assert_signals (SignalTarget *target,
                GSignalGroup *group,
                gboolean      success)
{
  g_assert (TEST_IS_SIGNAL_TARGET (target));
  g_assert (group == NULL || G_IS_SIGNAL_GROUP (group));

  global_signal_calls = 0;
  g_signal_emit (target, signals[THE_SIGNAL],
                 signal_detail_quark (), group);
  g_assert_cmpint (global_signal_calls, ==, success ? 6 : 0);
}

static void
dummy_handler (void)
{
}

static void
test_signal_group_invalid (void)
{
  GObject *invalid_target = g_object_new (G_TYPE_OBJECT, NULL);
  SignalTarget *target = g_object_new (signal_target_get_type (), NULL);
  GSignalGroup *group = g_signal_group_new (signal_target_get_type ());

  /* Invalid Target Type */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*g_type_is_a*G_TYPE_OBJECT*");
  g_signal_group_new (G_TYPE_DATE_TIME);
  g_test_assert_expected_messages ();

  /* Invalid Target */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*Failed to set GSignalGroup of target type SignalTarget using target * of type GObject*");
  g_signal_group_set_target (group, invalid_target);
  g_assert_finalize_object (group);
  g_test_assert_expected_messages ();

  /* Invalid Signal Name */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*Invalid signal name “does-not-exist”*");
  group = g_signal_group_new (signal_target_get_type ());
  g_signal_group_connect (group,
                          "does-not-exist",
                          G_CALLBACK (connect_before_cb),
                          NULL);
  g_test_assert_expected_messages ();
  g_assert_finalize_object (group);

  /* Invalid Callback */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*c_handler != NULL*");
  group = g_signal_group_new (signal_target_get_type ());
  g_signal_group_connect (group,
                          "the-signal",
                          G_CALLBACK (NULL),
                          NULL);
  g_test_assert_expected_messages ();
  g_assert_finalize_object (group);

  /* Connecting after setting target */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*Cannot add signals after setting target*");
  group = g_signal_group_new (signal_target_get_type ());
  g_signal_group_set_target (group, target);
  g_signal_group_connect (group,
                          "the-signal",
                          G_CALLBACK (dummy_handler),
                          NULL);
  g_test_assert_expected_messages ();
  g_assert_finalize_object (group);

  g_assert_finalize_object (target);
  g_assert_finalize_object (invalid_target);
}

static void
test_signal_group_simple (void)
{
  SignalTarget *target;
  GSignalGroup *group;
  SignalTarget *readback;

  /* Set the target before connecting the signals */
  group = g_signal_group_new (signal_target_get_type ());
  target = g_object_new (signal_target_get_type (), NULL);
  g_assert_null (g_signal_group_dup_target (group));
  g_signal_group_set_target (group, target);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target);
  g_object_unref (readback);
  g_assert_finalize_object (group);
  assert_signals (target, NULL, FALSE);
  g_assert_finalize_object (target);

  group = g_signal_group_new (signal_target_get_type ());
  target = g_object_new (signal_target_get_type (), NULL);
  connect_all_signals (group);
  g_signal_group_set_target (group, target);
  assert_signals (target, group, TRUE);
  g_assert_finalize_object (target);
  g_assert_finalize_object (group);
}

static void
test_signal_group_changing_target (void)
{
  SignalTarget *target1, *target2;
  GSignalGroup *group = g_signal_group_new (signal_target_get_type ());
  SignalTarget *readback;

  connect_all_signals (group);
  g_assert_null (g_signal_group_dup_target (group));

  /* Set the target after connecting the signals */
  target1 = g_object_new (signal_target_get_type (), NULL);
  g_signal_group_set_target (group, target1);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target1);
  g_object_unref (readback);

  assert_signals (target1, group, TRUE);

  /* Set the same target */
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target1);
  g_object_unref (readback);
  g_signal_group_set_target (group, target1);

  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target1);
  g_object_unref (readback);

  assert_signals (target1, group, TRUE);

  /* Set a new target when the current target is non-NULL */
  target2 = g_object_new (signal_target_get_type (), NULL);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target1);
  g_object_unref (readback);

  g_signal_group_set_target (group, target2);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target2);
  g_object_unref (readback);

  assert_signals (target2, group, TRUE);

  g_assert_finalize_object (target2);
  g_assert_finalize_object (target1);
  g_assert_finalize_object (group);
}

static void
assert_blocking (SignalTarget *target,
                 GSignalGroup *group,
                 gint          count)
{
  gint i;

  assert_signals (target, group, TRUE);

  /* Assert that multiple blocks are effective */
  for (i = 0; i < count; ++i)
    {
      g_signal_group_block (group);
      assert_signals (target, group, FALSE);
    }

  /* Assert that the signal is not emitted after the first unblock */
  for (i = 0; i < count; ++i)
    {
      assert_signals (target, group, FALSE);
      g_signal_group_unblock (group);
    }

  assert_signals (target, group, TRUE);
}

static void
test_signal_group_blocking (void)
{
  SignalTarget *target1, *target2, *readback;
  GSignalGroup *group = g_signal_group_new (signal_target_get_type ());

  /* Test blocking and unblocking null target */
  g_signal_group_block (group);
  g_signal_group_unblock (group);

  connect_all_signals (group);
  g_assert_null (g_signal_group_dup_target (group));

  target1 = g_object_new (signal_target_get_type (), NULL);
  g_signal_group_set_target (group, target1);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target1);
  g_object_unref (readback);

  assert_blocking (target1, group, 1);
  assert_blocking (target1, group, 3);
  assert_blocking (target1, group, 15);

  /* Assert that blocking transfers across changing the target */
  g_signal_group_block (group);
  g_signal_group_block (group);

  /* Set a new target when the current target is non-NULL */
  target2 = g_object_new (signal_target_get_type (), NULL);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target1);
  g_object_unref (readback);
  g_signal_group_set_target (group, target2);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target2);
  g_object_unref (readback);

  assert_signals (target2, group, FALSE);
  g_signal_group_unblock (group);
  assert_signals (target2, group, FALSE);
  g_signal_group_unblock (group);
  assert_signals (target2, group, TRUE);

  g_assert_finalize_object (target2);
  g_assert_finalize_object (target1);
  g_assert_finalize_object (group);
}

static void
test_signal_group_weak_ref_target (void)
{
  SignalTarget *target = g_object_new (signal_target_get_type (), NULL);
  GSignalGroup *group = g_signal_group_new (signal_target_get_type ());
  SignalTarget *readback;

  g_assert_null (g_signal_group_dup_target (group));
  g_signal_group_set_target (group, target);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target);
  g_object_unref (readback);

  g_assert_finalize_object (target);
  g_assert_null (g_signal_group_dup_target (group));
  g_assert_finalize_object (group);
}

static void
test_signal_group_connect_object (void)
{
  GObject *object = g_object_new (G_TYPE_OBJECT, NULL);
  SignalTarget *target = g_object_new (signal_target_get_type (), NULL);
  GSignalGroup *group = g_signal_group_new (signal_target_get_type ());
  SignalTarget *readback;

  /* We already do basic connect_object() tests in connect_signals(),
   * this is only needed to test the specifics of connect_object()
   */
  g_signal_group_connect_object (group,
                                 "the-signal",
                                 G_CALLBACK (connect_object_cb),
                                 object,
                                 0);

  g_assert_null (g_signal_group_dup_target (group));
  g_signal_group_set_target (group, target);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target);
  g_object_unref (readback);

  g_assert_finalize_object (object);

  /* This would cause a warning if the SignalGroup did not
   * have a weakref on the object as it would try to connect again
   */
  g_signal_group_set_target (group, NULL);
  g_assert_null (g_signal_group_dup_target (group));
  g_signal_group_set_target (group, target);
  readback = g_signal_group_dup_target (group);
  g_assert_true (readback == target);
  g_object_unref (readback);

  g_assert_finalize_object (group);
  g_assert_finalize_object (target);
}

static void
test_signal_group_signal_parsing (void)
{
  g_test_trap_subprocess ("/GObject/SignalGroup/signal-parsing/subprocess", 0,
                          G_TEST_SUBPROCESS_INHERIT_STDERR);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("");
}

static void
test_signal_group_signal_parsing_subprocess (void)
{
  GSignalGroup *group;

  /* Check that the class has not been created and with it the
   * signals registered. This will cause g_signal_parse_name()
   * to fail unless GSignalGroup calls g_type_class_ref().
   */
  g_assert_null (g_type_class_peek (signal_target_get_type ()));

  group = g_signal_group_new (signal_target_get_type ());
  g_signal_group_connect (group,
                          "the-signal",
                          G_CALLBACK (connect_before_cb),
                          NULL);

  g_assert_finalize_object (group);
}

static void
test_signal_group_properties (void)
{
  GSignalGroup *group;
  SignalTarget *target, *other;
  GType gtype;

  group = g_signal_group_new (signal_target_get_type ());
  g_object_get (group,
                "target", &target,
                "target-type", &gtype,
                NULL);
  g_assert_cmpint ((uintptr_t) gtype, ==, (uintptr_t) signal_target_get_type ());
  g_assert_null (target);

  target = g_object_new (signal_target_get_type (), NULL);
  g_object_set (group, "target", target, NULL);
  g_object_get (group, "target", &other, NULL);
  g_assert_true (target == other);
  g_object_unref (other);

  g_assert_finalize_object (target);
  g_assert_null (g_signal_group_dup_target (group));
  g_assert_finalize_object (group);
}

G_DECLARE_INTERFACE (SignalThing, signal_thing, SIGNAL, THING, GObject)

struct _SignalThingInterface
{
  GTypeInterface iface;
  void (*changed) (SignalThing *thing);
};

G_DEFINE_INTERFACE (SignalThing, signal_thing, G_TYPE_OBJECT)

static guint signal_thing_changed;

static void
signal_thing_default_init (SignalThingInterface *iface)
{
  signal_thing_changed =
      g_signal_new ("changed",
                    G_TYPE_FROM_INTERFACE (iface),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (SignalThingInterface, changed),
                    NULL, NULL, NULL,
                    G_TYPE_NONE, 0);
}

G_GNUC_NORETURN static void
thing_changed_cb (SignalThing *thing,
                  gpointer     user_data G_GNUC_UNUSED)
{
  g_assert_not_reached ();
}

static void
test_signal_group_interface (void)
{
  GSignalGroup *group;

  group = g_signal_group_new (signal_thing_get_type ());
  g_signal_group_connect (group,
                          "changed",
                          G_CALLBACK (thing_changed_cb),
                          NULL);
  g_assert_finalize_object (group);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/GObject/SignalGroup/invalid", test_signal_group_invalid);
  g_test_add_func ("/GObject/SignalGroup/simple", test_signal_group_simple);
  g_test_add_func ("/GObject/SignalGroup/changing-target", test_signal_group_changing_target);
  g_test_add_func ("/GObject/SignalGroup/blocking", test_signal_group_blocking);
  g_test_add_func ("/GObject/SignalGroup/weak-ref-target", test_signal_group_weak_ref_target);
  g_test_add_func ("/GObject/SignalGroup/connect-object", test_signal_group_connect_object);
  g_test_add_func ("/GObject/SignalGroup/signal-parsing", test_signal_group_signal_parsing);
  g_test_add_func ("/GObject/SignalGroup/signal-parsing/subprocess", test_signal_group_signal_parsing_subprocess);
  g_test_add_func ("/GObject/SignalGroup/properties", test_signal_group_properties);
  g_test_add_func ("/GObject/SignalGroup/interface", test_signal_group_interface);
  return g_test_run ();
}
