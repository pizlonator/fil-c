#include <gtk/gtk.h>

#define GTK_COMPILATION
#include "gdk/gdkeventsprivate.h"

static GdkEvent *
key_event_new (GdkEventType      event_type,
               GdkSurface       *surface,
               GdkDevice        *device,
               GdkDevice        *source_device,
               guint32           time_,
               guint             keycode,
               GdkModifierType   state,
               gboolean          is_modifier,
               GdkTranslatedKey *translated,
               GdkTranslatedKey *no_lock)
{
  GdkKeyEvent *key_event = (GdkKeyEvent *) g_type_create_instance (GDK_TYPE_KEY_EVENT);
  GdkEvent *event = (GdkEvent *) key_event;

  event->event_type = event_type;
  event->surface = g_object_ref (surface);
  event->device = g_object_ref (device);
  event->time = time_;

  key_event->keycode = keycode;
  key_event->state = state;
  key_event->key_is_modifier = is_modifier;
  key_event->translated[0] = *translated;
  key_event->translated[1] = *no_lock;

  return event;
}

static void
test_trigger_basic (void)
{
  GtkShortcutTrigger *trigger;

  trigger = gtk_never_trigger_get ();

  g_assert_true (GTK_IS_SHORTCUT_TRIGGER (trigger));

  trigger = gtk_keyval_trigger_new (GDK_KEY_a, GDK_CONTROL_MASK);
  g_assert_cmpint (gtk_keyval_trigger_get_keyval (GTK_KEYVAL_TRIGGER (trigger)), ==, GDK_KEY_a);
  g_assert_cmpint (gtk_keyval_trigger_get_modifiers (GTK_KEYVAL_TRIGGER (trigger)), ==, GDK_CONTROL_MASK);
  g_object_unref (trigger);

  trigger = gtk_mnemonic_trigger_new (GDK_KEY_u);
  g_assert_cmpint (gtk_mnemonic_trigger_get_keyval (GTK_MNEMONIC_TRIGGER (trigger)), ==, GDK_KEY_u);
  g_object_unref (trigger);
}

static void
test_trigger_equal (void)
{
  GtkShortcutTrigger *trigger1, *trigger2, *trigger3, *trigger4;
  GtkShortcutTrigger *trigger5, *trigger6, *trigger1a, *trigger2a;

  trigger1 = gtk_keyval_trigger_new ('u', GDK_CONTROL_MASK);
  trigger2 = g_object_ref (gtk_never_trigger_get ());
  trigger3 = gtk_alternative_trigger_new (g_object_ref (trigger1), g_object_ref (trigger2));
  trigger4 = gtk_alternative_trigger_new (g_object_ref (trigger2), g_object_ref (trigger1));
  trigger5 = gtk_keyval_trigger_new ('u', GDK_SHIFT_MASK);
  trigger6 = gtk_mnemonic_trigger_new ('u');

  trigger1a = gtk_keyval_trigger_new ('u', GDK_CONTROL_MASK);
  trigger2a = g_object_ref (gtk_never_trigger_get ());

  g_assert_true (gtk_shortcut_trigger_equal (trigger1, trigger1));
  g_assert_true (gtk_shortcut_trigger_equal (trigger2, trigger2));
  g_assert_true (gtk_shortcut_trigger_equal (trigger3, trigger3));
  g_assert_true (gtk_shortcut_trigger_equal (trigger4, trigger4));
  g_assert_true (gtk_shortcut_trigger_equal (trigger5, trigger5));
  g_assert_true (gtk_shortcut_trigger_equal (trigger6, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger2));
  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger3));
  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger4));
  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger5));
  g_assert_false (gtk_shortcut_trigger_equal (trigger1, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger2, trigger3));
  g_assert_false (gtk_shortcut_trigger_equal (trigger2, trigger4));
  g_assert_false (gtk_shortcut_trigger_equal (trigger2, trigger5));
  g_assert_false (gtk_shortcut_trigger_equal (trigger2, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger3, trigger4));
  g_assert_false (gtk_shortcut_trigger_equal (trigger3, trigger5));
  g_assert_false (gtk_shortcut_trigger_equal (trigger3, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger4, trigger5));
  g_assert_false (gtk_shortcut_trigger_equal (trigger4, trigger6));

  g_assert_false (gtk_shortcut_trigger_equal (trigger5, trigger6));

  g_assert_true (gtk_shortcut_trigger_equal (trigger1, trigger1a));
  g_assert_true (gtk_shortcut_trigger_equal (trigger2, trigger2a));

  g_object_unref (trigger1);
  g_object_unref (trigger2);
  g_object_unref (trigger3);
  g_object_unref (trigger4);
  g_object_unref (trigger5);
  g_object_unref (trigger6);
  g_object_unref (trigger1a);
  g_object_unref (trigger2a);
}

static void
test_trigger_parse_never (void)
{
  GtkShortcutTrigger *trigger;

  trigger = gtk_shortcut_trigger_parse_string ("never");
  g_assert_true (GTK_IS_NEVER_TRIGGER (trigger));

  g_object_unref (trigger);
}

static void
test_trigger_parse_keyval (void)
{
  const struct
  {
    const char *str;
    GdkModifierType modifiers;
    guint keyval;
    int trigger_type;
  } tests[] = {
    { "<Primary><Alt>z", GDK_CONTROL_MASK | GDK_ALT_MASK, 'z' },
    { "<Control>U", GDK_CONTROL_MASK, 'u' },
    { "<Hyper>x", GDK_HYPER_MASK, 'x' },
    { "<Meta>y", GDK_META_MASK, 'y' },
    { "KP_7", 0, GDK_KEY_KP_7 },
    { "<Shift>exclam", GDK_SHIFT_MASK, '!' },
  };

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_test_message ("Checking: '%s'", tests[i].str);

      GtkShortcutTrigger *trigger = gtk_shortcut_trigger_parse_string (tests[i].str);

      g_assert_true (GTK_IS_KEYVAL_TRIGGER (trigger));
      g_assert_cmpint (gtk_keyval_trigger_get_modifiers (GTK_KEYVAL_TRIGGER (trigger)),
                       ==,
                       tests[i].modifiers);
      g_assert_cmpuint (gtk_keyval_trigger_get_keyval (GTK_KEYVAL_TRIGGER (trigger)),
                        ==,
                        tests[i].keyval);
      g_object_unref (trigger);
    }
}

static void
test_trigger_parse_mnemonic (void)
{
  struct
  {
    const char *str;
    guint keyval;
  } tests[] = {
    { "_A", GDK_KEY_a },
    { "_s", GDK_KEY_s },
  };

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_test_message ("Checking: '%s'", tests[i].str);

      GtkShortcutTrigger *trigger = gtk_shortcut_trigger_parse_string (tests[i].str);

      g_assert_true (GTK_IS_MNEMONIC_TRIGGER (trigger));
      g_assert_cmpuint (gtk_mnemonic_trigger_get_keyval (GTK_MNEMONIC_TRIGGER (trigger)),
                        ==,
                        tests[i].keyval);
      g_object_unref (trigger);
    }
}

static void
test_trigger_parse_alternative (void)
{
  enum
  {
    TRIGGER_NEVER,
    TRIGGER_KEYVAL,
    TRIGGER_MNEMONIC,
    TRIGGER_ALTERNATIVE
  };

  const struct
  {
    const char *str;
    int first;
    int second;
  } tests[] = {
    { "U|<Primary>U", TRIGGER_KEYVAL, TRIGGER_KEYVAL },
    { "_U|<Shift>u", TRIGGER_MNEMONIC, TRIGGER_KEYVAL },
    { "x|_x|<Primary>x", TRIGGER_KEYVAL, TRIGGER_ALTERNATIVE },
  };

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_test_message ("Checking: '%s'", tests[i].str);

      GtkShortcutTrigger *trigger = gtk_shortcut_trigger_parse_string (tests[i].str);

      g_assert_true (GTK_IS_ALTERNATIVE_TRIGGER (trigger));

      GtkShortcutTrigger *t1 = gtk_alternative_trigger_get_first (GTK_ALTERNATIVE_TRIGGER (trigger));

      switch (tests[i].first)
        {
        case TRIGGER_NEVER:
          g_assert_true (GTK_IS_NEVER_TRIGGER (t1));
          break;

        case TRIGGER_KEYVAL:
          g_assert_true (GTK_IS_KEYVAL_TRIGGER (t1));
          break;

        case TRIGGER_MNEMONIC:
          g_assert_true (GTK_IS_MNEMONIC_TRIGGER (t1));
          break;

        case TRIGGER_ALTERNATIVE:
          g_assert_true (GTK_IS_ALTERNATIVE_TRIGGER (t1));
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      GtkShortcutTrigger *t2 = gtk_alternative_trigger_get_second (GTK_ALTERNATIVE_TRIGGER (trigger));

      switch (tests[i].second)
        {
        case TRIGGER_NEVER:
          g_assert_true (GTK_IS_NEVER_TRIGGER (t2));
          break;

        case TRIGGER_KEYVAL:
          g_assert_true (GTK_IS_KEYVAL_TRIGGER (t2));
          break;

        case TRIGGER_MNEMONIC:
          g_assert_true (GTK_IS_MNEMONIC_TRIGGER (t2));
          break;

        case TRIGGER_ALTERNATIVE:
          g_assert_true (GTK_IS_ALTERNATIVE_TRIGGER (t2));
          break;

        default:
          g_assert_not_reached ();
          break;
        }

      g_object_unref (trigger);
    }
}

static void
test_trigger_parse_invalid (void)
{
  const char *tests[] = {
    "<never>",
    "Never",
    "Foo",
    "<Foo>Nyaa",
    "never|",
    "|never",
  };

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_test_message ("Checking: '%s'", tests[i]);

      GtkShortcutTrigger *trigger = gtk_shortcut_trigger_parse_string (tests[i]);

      g_assert_null (trigger);
    }
}

static void
test_trigger_trigger (void)
{
  GtkShortcutTrigger *trigger[4];
  GdkDisplay *display;
  GdkSeat *seat;
  GdkSurface *surface;
  GdkDevice *device;
  GdkEvent *event;
  struct {
    guint keyval;
    GdkModifierType state;
    gboolean mnemonic;
    GdkKeyMatch result[4];
  } tests[] = {
    { GDK_KEY_a, GDK_CONTROL_MASK, FALSE, { GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_EXACT, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_EXACT } }, 
    { GDK_KEY_a, GDK_CONTROL_MASK, TRUE,  { GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_EXACT, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_EXACT } }, 
    { GDK_KEY_a, GDK_SHIFT_MASK,   FALSE, { GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE } }, 
    { GDK_KEY_a, GDK_SHIFT_MASK,   TRUE,  { GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE } }, 
    { GDK_KEY_u, GDK_SHIFT_MASK,   FALSE, { GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE } }, 
    { GDK_KEY_u, GDK_SHIFT_MASK,   TRUE,  { GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_NONE, GDK_KEY_MATCH_EXACT, GDK_KEY_MATCH_EXACT } }, 
  };
  int i, j;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);
  if (!seat)
    {
      g_test_skip ("Display has no seat");
      return;
    }

  trigger[0] = g_object_ref (gtk_never_trigger_get ());
  trigger[1] = gtk_keyval_trigger_new (GDK_KEY_a, GDK_CONTROL_MASK);
  trigger[2] = gtk_mnemonic_trigger_new (GDK_KEY_u);
  trigger[3] = gtk_alternative_trigger_new (g_object_ref (trigger[1]),
                                            g_object_ref (trigger[2]));

  device = gdk_seat_get_keyboard (seat);
  surface = gdk_surface_new_toplevel (display);

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      GdkKeymapKey *keys;
      int n_keys;
      GdkTranslatedKey translated;

      if (!gdk_display_map_keyval (display, tests[i].keyval, &keys, &n_keys))
        continue;

      translated.keyval = tests[i].keyval;
      translated.consumed = 0;
      translated.layout = keys[0].group;
      translated.level = keys[0].level;
      event = key_event_new (GDK_KEY_PRESS,
                             surface,
                             device,
                             device,
                             GDK_CURRENT_TIME,
                             keys[0].keycode,
                             tests[i].state,
                             FALSE,
                             &translated,
                             &translated);
      for (j = 0; j < 4; j++)
        {
          g_assert_cmpint (gtk_shortcut_trigger_trigger (trigger[j], event, tests[i].mnemonic), ==, tests[i].result[j]);
        }

      gdk_event_unref (event);

      g_free (keys);
    }

  gdk_surface_destroy (surface);

  g_object_unref (trigger[0]);
  g_object_unref (trigger[1]);
  g_object_unref (trigger[2]);
  g_object_unref (trigger[3]);
}

static gboolean
callback (GtkWidget *widget,
          GVariant  *args,
          gpointer   user_data)
{
  int *callback_count = user_data;
  *callback_count += 1;
  return TRUE;
}

static void
test_action_basic (void)
{
  GtkShortcutAction *action;

  action = gtk_signal_action_new ("activate");
  g_assert_cmpstr (gtk_signal_action_get_signal_name (GTK_SIGNAL_ACTION (action)), ==, "activate");
  g_object_unref (action);

  action = gtk_named_action_new ("text.undo");
  g_assert_cmpstr (gtk_named_action_get_action_name (GTK_NAMED_ACTION (action)), ==, "text.undo");
  g_object_unref (action);
}

static void
test_action_activate (void)
{
  GtkShortcutAction *action;
  GtkWidget *widget;
  int callback_count;

  widget = gtk_label_new ("");
  g_object_ref_sink (widget);

  action = gtk_nothing_action_get ();
  g_assert_false (gtk_shortcut_action_activate (action, 0, widget, NULL));

  callback_count = 0;
  action = gtk_callback_action_new (callback, &callback_count, NULL);
  g_assert_true (gtk_shortcut_action_activate (action, 0, widget, NULL));
  g_assert_cmpint (callback_count, ==, 1);
  g_object_unref (action);

  g_object_unref (widget);
}

static void
test_action_parse (void)
{
  GtkShortcutAction *action;

  action = gtk_shortcut_action_parse_string ("nothing");
  g_assert_true (GTK_IS_NOTHING_ACTION (action));
  g_object_unref (action);

  action = gtk_shortcut_action_parse_string ("activate");
  g_assert_true (GTK_IS_ACTIVATE_ACTION (action));
  g_object_unref (action);

  action = gtk_shortcut_action_parse_string ("mnemonic-activate");
  g_assert_true (GTK_IS_MNEMONIC_ACTION (action));
  g_object_unref (action);

  action = gtk_shortcut_action_parse_string ("action(win.dark)");
  g_assert_true (GTK_IS_NAMED_ACTION (action));
  g_object_unref (action);

  action = gtk_shortcut_action_parse_string ("signal(frob)");
  g_assert_true (GTK_IS_SIGNAL_ACTION (action));
  g_object_unref (action);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/shortcuts/trigger/basic", test_trigger_basic);
  g_test_add_func ("/shortcuts/trigger/equal", test_trigger_equal);
  g_test_add_func ("/shortcuts/trigger/parse/never", test_trigger_parse_never);
  g_test_add_func ("/shortcuts/trigger/parse/keyval", test_trigger_parse_keyval);
  g_test_add_func ("/shortcuts/trigger/parse/mnemonic", test_trigger_parse_mnemonic);
  g_test_add_func ("/shortcuts/trigger/parse/alternative", test_trigger_parse_alternative);
  g_test_add_func ("/shortcuts/trigger/parse/invalid", test_trigger_parse_invalid);
  g_test_add_func ("/shortcuts/trigger/trigger", test_trigger_trigger);
  g_test_add_func ("/shortcuts/action/basic", test_action_basic);
  g_test_add_func ("/shortcuts/action/activate", test_action_activate);
  g_test_add_func ("/shortcuts/action/parse", test_action_parse);

  return g_test_run ();
}
