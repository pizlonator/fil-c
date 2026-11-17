#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <locale.h>
#include <stdlib.h>
#include "keyboard-shortcuts.h"

#define NUM_LAYOUTS 4

typedef struct _shortcut_test {
  guint16 keycode;
  GdkModifierType modifiers;
  char *expected_results[NUM_LAYOUTS];
} shortcut_test;

/* keycodes taken from /usr/share/X11/xkb/keycodes/evdev */
shortcut_test shortcut_tests[] = {
  {
   24 /* first key after tab in first letter row */,
   GDK_SHIFT_MASK | GDK_SUPER_MASK,
     {
       "<Shift><Super>q" /* us */,
       "<Shift><Super>apostrophe" /* us+dvorak */,
       "<Shift><Super>a" /* fr+azerty */,
       "<Shift><Super>Cyrillic_shorti" /* ru */
       /* "<Shift><Super>q" would be valid, too */,
     },
  },
  {
   13 /* fifth key in num row */,
   GDK_SUPER_MASK,
     {
       "<Super>4" /* us */,
       "<Super>4" /* us+dvorak */,
       "<Super>4" /* fr+azerty */,
       "<Super>4" /* ru */,
     },
  },
  {
   13 /* fifth key in num row */,
   GDK_SHIFT_MASK | GDK_SUPER_MASK,
     {
       "<Shift><Super>4" /* us */,
       "<Shift><Super>4" /* us+dvorak */,
       "<Shift><Super>4" /* fr+azerty */,
       "<Shift><Super>4" /* ru */,
     },
  },
  {
   65 /* space key */,
   GDK_SHIFT_MASK | GDK_SUPER_MASK,
     {
       "<Shift><Super>space" /* us */,
       "<Shift><Super>space" /* us+dvorak */,
       "<Shift><Super>space" /* fr+azerty */,
       "<Shift><Super>space" /* ru */,
     },
  },
  {
   23 /* tab key */,
   GDK_SHIFT_MASK | GDK_SUPER_MASK,
     {
       "<Shift><Super>Tab" /* us */,
       "<Shift><Super>Tab" /* us+dvorak */,
       "<Shift><Super>Tab" /* fr+azerty */,
       "<Shift><Super>Tab" /* ru */,
     },
  },
  {
   107 /* print screen/sysrq key */,
   GDK_ALT_MASK,
     {
       "<Alt>Print" /* us */,
       "<Alt>Print" /* us+dvorak */,
       "<Alt>Print" /* fr+azerty */,
       "<Alt>Print" /* ru */,
     },
  },
};

static void
test_event_translation (shortcut_test *shortcut_test)
{
  g_autofree char *translated_name = NULL;
  guint keyval;
  GdkModifierType modifiers;

  for (int group = 0; group < NUM_LAYOUTS; group++)
    {
      if (!shortcut_test->expected_results[group])
        continue;

      normalize_keyval_and_mask (shortcut_test->keycode,
                                 shortcut_test->modifiers,
                                 group,
                                 &keyval,
                                 &modifiers);

      translated_name = gtk_accelerator_name (keyval, modifiers);

      if (g_strcmp0 (translated_name, shortcut_test->expected_results[group]) != 0)
        {
          g_error ("Result for keycode %u with modifieres %u for "
                   "group %d doesn't match '%s' (got: '%s')",
                   shortcut_test->keycode,
                   shortcut_test->modifiers,
                   group,
                   shortcut_test->expected_results[group],
                   translated_name);
          g_test_fail ();
        }
    }
}

static void
set_keyboard_layouts (char *layouts,
                      char *variants,
                      char *options)
{
  GSubprocess *proc;
  GError *error = NULL;

  proc = g_subprocess_new (G_SUBPROCESS_FLAGS_NONE,
                           &error,
                           "setxkbmap",
                           "-layout", layouts,
                           "-variant", variants,
                           "-option", options,
                           "-model", "pc105",
                           NULL);

  if (!proc || !g_subprocess_wait_check(proc, NULL, &error))
    {
      g_critical ("Failed to set layout: %s", error->message);
      exit (1);
    }

  g_object_unref (proc);
}

static void
run_shortcut_tests (void)
{
  set_keyboard_layouts ("us,us,fr,ru", ",dvorak,azerty,", "");

  for (int i = 0; i < G_N_ELEMENTS(shortcut_tests); i++)
    test_event_translation (&shortcut_tests[i]);
}

int main (int argc, char **argv)
{
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("GDK_BACKEND", "x11", TRUE);
  g_setenv ("LC_ALL", "C", TRUE);

  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/keyboard/shortcut-translation",
                   run_shortcut_tests);

  return g_test_run ();
}
