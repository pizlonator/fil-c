/* accel.c - test accel parsing
 * Copyright (C) 2011 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>
#include <locale.h>

static void
test_one_accel (const char      *accel,
                GdkModifierType  exp_mods,
                guint            exp_key,
		const char      *exp_label,
		gboolean         has_keysym)
{
  guint accel_key;
  GdkModifierType mods;
  guint *keycodes;
  char *label, *name;
  gboolean ret;

  accel_key = 0;
  ret = gtk_accelerator_parse_with_keycode (accel,
                                            gdk_display_get_default (),
                                            &accel_key,
                                            &keycodes,
                                            &mods);
  g_assert_true (ret);

  if (has_keysym)
    {
      guint accel_key_2;
      GdkModifierType mods_2;

      ret = gtk_accelerator_parse (accel, &accel_key_2, &mods_2);
      g_assert_true (ret);
      g_assert_true (accel_key == accel_key_2);
      g_assert_true (mods == mods_2);
    }

  if (has_keysym)
    g_assert_true (accel_key == exp_key);
  g_assert_true (mods == exp_mods);
  g_assert_nonnull (keycodes);
  g_assert_true (keycodes[0] != 0);

  label = gtk_accelerator_get_label_with_keycode (NULL,
						  accel_key,
						  *keycodes,
						  mods);

  g_assert_cmpstr (label, ==, exp_label);

  name = gtk_accelerator_name_with_keycode (NULL,
					    accel_key,
					    *keycodes,
					    mods);
  g_assert_cmpstr (name, ==, accel);

  g_free (keycodes);
  g_free (label);
  g_free (name);
}

static void
accel1 (void)
{
  test_one_accel ("0xb3", 0, 0xb3, "0xb3", FALSE);
}

static void
accel2 (void)
{
  test_one_accel ("<Control><Alt>z", GDK_CONTROL_MASK|GDK_ALT_MASK, GDK_KEY_z, "Ctrl+Alt+Z", TRUE);
}

static void
accel3 (void)
{
  test_one_accel ("KP_7", 0, GDK_KEY_KP_7, "KP 7", TRUE);
}

static void
accel4 (void)
{
  test_one_accel ("<Control>KP_7", GDK_CONTROL_MASK, GDK_KEY_KP_7, "Ctrl+KP 7", TRUE);
}

static void
accel5 (void)
{
  test_one_accel ("<Shift>exclam", GDK_SHIFT_MASK, GDK_KEY_exclam, "Shift+!", TRUE);
}

static void
accel6 (void)
{
  test_one_accel ("<Hyper>x", GDK_HYPER_MASK, GDK_KEY_x, "Hyper+X", TRUE);
}

static void
accel7 (void)
{
  test_one_accel ("<Super>x", GDK_SUPER_MASK, GDK_KEY_x, "Super+X", TRUE);
}

static void
accel8 (void)
{
  test_one_accel ("<Meta>x", GDK_META_MASK, GDK_KEY_x, "Meta+X", TRUE);
}

static void
keysyms (void)
{
  g_assert_cmpuint (gdk_keyval_from_name ("KP_7"), ==, GDK_KEY_KP_7);
}

int
main (int   argc,
      char *argv[])
{
  setlocale (LC_ALL, "en_GB.UTF-8");

  gtk_test_init (&argc, &argv);

  g_test_add_func ("/keysyms", keysyms);

  g_test_add_func ("/accel1", accel1);
  g_test_add_func ("/accel2", accel2);
  g_test_add_func ("/accel3", accel3);
  g_test_add_func ("/accel4", accel4);
  g_test_add_func ("/accel5", accel5);
  g_test_add_func ("/accel6", accel6);
  g_test_add_func ("/accel7", accel7);
  g_test_add_func ("/accel8", accel8);

  return g_test_run();
}
