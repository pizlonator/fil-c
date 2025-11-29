/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "config.h"

#include <gdk/gdk.h>

#include <stdlib.h>
#include <string.h>

#include "gtkprivate.h"
#include "gtkaccelgroup.h"
#include "gtkimcontextsimple.h"
#include "gtksettings.h"
#include "gtkwidget.h"
#include "gtkdebug.h"
#include "gtkcomposetable.h"
#include "gtkimmoduleprivate.h"

#include "gtkimcontextsimpleseqs.h"

#include "gdk/gdkeventsprivate.h"
#include "gdk/gdkprofilerprivate.h"

/**
 * GtkIMContextSimple:
 *
 * `GtkIMContextSimple` is an input method supporting table-based input methods.
 *
 * ## Compose sequences
 *
 * `GtkIMContextSimple` reads compose sequences from the first of the
 * following files that is found: ~/.config/gtk-4.0/Compose, ~/.XCompose,
 * /usr/share/X11/locale/$locale/Compose (for locales that have a nontrivial
 * Compose file). A subset of the file syntax described in the Compose(5)
 * manual page is supported. Additionally, `include "%L"` loads GTK’s built-in
 * table of compose sequences rather than the locale-specific one from X11.
 *
 * If none of these files is found, `GtkIMContextSimple` uses a built-in table
 * of compose sequences that is derived from the X11 Compose files.
 *
 * Note that compose sequences typically start with the Compose_key, which is
 * often not available as a dedicated key on keyboards. Keyboard layouts may
 * map this keysym to other keys, such as the right Control key.
 *
 * ## Unicode characters
 *
 * `GtkIMContextSimple` also supports numeric entry of Unicode characters
 * by typing <kbd>Ctrl</kbd>-<kbd>Shift</kbd>-<kbd>u</kbd>, followed by a
 * hexadecimal Unicode codepoint.
 *
 * For example,
 *
 *     Ctrl-Shift-u 1 2 3 Enter
 *
 * yields U+0123 LATIN SMALL LETTER G WITH CEDILLA, i.e. ģ.
 *
 * ## Dead keys
 *
 * `GtkIMContextSimple` supports dead keys. For example, typing
 *
 *     dead_acute a
 *
 *  yields U+00E! LATIN SMALL LETTER_A WITH ACUTE, i.e. á. Note that this
 *  depends on the keyboard layout including dead keys.
 */

struct _GtkIMContextSimplePrivate
{
  guint         *compose_buffer;
  int            compose_buffer_len;
  GString       *tentative_match;
  int            tentative_match_len;

  guint          in_hex_sequence : 1;
  guint          in_compose_sequence : 1;
  guint          modifiers_dropped : 1;
};

#include "gtk/compose/gtkcomposedata.h"

GtkComposeTable builtin_compose_table = {
  NULL,
  NULL,
  MAX_SEQ_LEN,
  N_INDEX_SIZE,
  DATA_SIZE,
  N_CHARS,
  0
};

static void
init_builtin_table (void)
{
  GBytes *bytes;

  bytes = g_resources_lookup_data ("/org/gtk/libgtk/compose/sequences", 0, NULL);
  builtin_compose_table.data = (guint16 *) g_bytes_get_data (bytes, NULL);
  g_bytes_unref (bytes);

  bytes = g_resources_lookup_data ("/org/gtk/libgtk/compose/chars", 0, NULL);
  builtin_compose_table.char_data = (char *) g_bytes_get_data (bytes, NULL);
  g_bytes_unref (bytes);
}

G_LOCK_DEFINE_STATIC (global_tables);
static GSList *global_tables;

static const guint gtk_compose_ignore[] = {
  0, /* Yes, XKB will send us key press events with NoSymbol :( */
  GDK_KEY_Overlay1_Enable,
  GDK_KEY_Overlay2_Enable,
  GDK_KEY_Shift_L,
  GDK_KEY_Shift_R,
  GDK_KEY_Control_L,
  GDK_KEY_Control_R,
  GDK_KEY_Caps_Lock,
  GDK_KEY_Shift_Lock,
  GDK_KEY_Meta_L,
  GDK_KEY_Meta_R,
  GDK_KEY_Alt_L,
  GDK_KEY_Alt_R,
  GDK_KEY_Super_L,
  GDK_KEY_Super_R,
  GDK_KEY_Hyper_L,
  GDK_KEY_Hyper_R,
  GDK_KEY_Mode_switch,
  GDK_KEY_ISO_Level3_Shift,
  GDK_KEY_ISO_Level3_Latch,
  GDK_KEY_ISO_Level5_Shift,
  GDK_KEY_ISO_Level5_Latch
};

static void     gtk_im_context_simple_finalize           (GObject                  *obj);
static gboolean gtk_im_context_simple_filter_keypress    (GtkIMContext             *context,
							  GdkEvent                 *key);
static void     gtk_im_context_simple_reset              (GtkIMContext             *context);
static void     gtk_im_context_simple_get_preedit_string (GtkIMContext             *context,
							  char                    **str,
							  PangoAttrList           **attrs,
							  int                      *cursor_pos);

static void init_compose_table_async (GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);

G_DEFINE_TYPE_WITH_CODE (GtkIMContextSimple, gtk_im_context_simple, GTK_TYPE_IM_CONTEXT,
                         G_ADD_PRIVATE (GtkIMContextSimple)
                         gtk_im_module_ensure_extension_point ();
                         g_io_extension_point_implement (GTK_IM_MODULE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "gtk-im-context-simple",
                                                         G_MININT))

static void
gtk_im_context_simple_class_init (GtkIMContextSimpleClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  im_context_class->filter_keypress = gtk_im_context_simple_filter_keypress;
  im_context_class->reset = gtk_im_context_simple_reset;
  im_context_class->get_preedit_string = gtk_im_context_simple_get_preedit_string;
  gobject_class->finalize = gtk_im_context_simple_finalize;

  init_builtin_table ();
  init_compose_table_async (NULL, NULL, NULL);
}

static int
gtk_compose_table_find (gconstpointer data1,
                        gconstpointer data2)
{
  const GtkComposeTable *compose_table = (const GtkComposeTable *) data1;
  guint32 hash = (guint32) GPOINTER_TO_INT (data2);
  return compose_table->id != hash;
}

static gboolean
add_compose_table_from_file (const char *compose_file)
{
  guint hash;
  gboolean ret = FALSE;

  G_LOCK (global_tables);

  hash = g_str_hash (compose_file);
  if (!g_slist_find_custom (global_tables, GINT_TO_POINTER (hash), gtk_compose_table_find))
    {
      GtkComposeTable *table;

      table = gtk_compose_table_new_with_file (compose_file);

      if (table)
        {
          ret = TRUE;
          global_tables = g_slist_prepend (global_tables, table);
        }
    }

  G_UNLOCK (global_tables);

  return ret;
}

static void
add_builtin_compose_table (void)
{
  G_LOCK (global_tables);

  global_tables = g_slist_prepend (global_tables, &builtin_compose_table);

  G_UNLOCK (global_tables);
}

static void
add_compose_table_from_data (guint16 *data,
                             int      max_seq_len,
                             int      n_seqs)
{
  guint hash;

  G_LOCK (global_tables);

  hash = gtk_compose_table_data_hash (data, max_seq_len, n_seqs);
  if (!g_slist_find_custom (global_tables, GINT_TO_POINTER (hash), gtk_compose_table_find))
    {
      GtkComposeTable *table;

      table = gtk_compose_table_new_with_data (data, max_seq_len, n_seqs);

      if (table)
        global_tables = g_slist_prepend (global_tables, table);
    }

  G_UNLOCK (global_tables);
}

static void
gtk_im_context_simple_init_compose_table (void)
{
  char *path = NULL;
  const char *home;
  const char *locale;
  char **langs = NULL;
  char **lang = NULL;
  const char * const sys_langs[] = { "el_gr", "fi_fi", "pt_br", NULL };
  const char * const *sys_lang = NULL;

  path = g_build_filename (g_get_user_config_dir (), "gtk-4.0", "Compose", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
      if (add_compose_table_from_file (path))
        {
          g_free (path);
          return;
        }
    }
  g_clear_pointer (&path, g_free);

  home = g_get_home_dir ();
  if (home == NULL)
    return;

  path = g_build_filename (home, ".XCompose", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
      if (add_compose_table_from_file (path))
        {
          g_free (path);
          return;
        }
    }
  g_clear_pointer (&path, g_free);

  locale = g_getenv ("LC_CTYPE");
  if (locale == NULL)
    locale = g_getenv ("LANG");
  if (locale == NULL)
    locale = "C";

  /* FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=751826 */
  langs = g_get_locale_variants (locale);

  for (lang = langs; *lang; lang++)
    {
      if (g_str_has_prefix (*lang, "en_US"))
        break;
      if (**lang == 'C')
        break;

      /* Other languages just include en_us compose table. */
      for (sys_lang = sys_langs; *sys_lang; sys_lang++)
        {
          if (g_ascii_strncasecmp (*lang, *sys_lang, strlen (*sys_lang)) == 0)
            {
              char *x11_compose_file_dir = gtk_compose_table_get_x11_compose_file_dir ();
              path = g_build_filename (x11_compose_file_dir, *lang, "Compose", NULL);
              g_free (x11_compose_file_dir);
              break;
            }
        }

      if (path == NULL)
        continue;

      if (g_file_test (path, G_FILE_TEST_EXISTS))
        break;
      g_clear_pointer (&path, g_free);
    }

  g_strfreev (langs);

  if (path != NULL &&
      add_compose_table_from_file (path))
    {
      g_free (path);
      return;
    }
  g_clear_pointer (&path, g_free);

  add_builtin_compose_table ();
}

static void
init_compose_table_thread_cb (GTask            *task,
                              gpointer          source_object,
                              gpointer          task_data,
                              GCancellable     *cancellable)
{
  gint64 before G_GNUC_UNUSED;

  before = GDK_PROFILER_CURRENT_TIME;

  if (g_task_return_error_if_cancelled (task))
    return;

  gtk_im_context_simple_init_compose_table ();

  g_task_return_boolean (task, TRUE);

  gdk_profiler_end_mark (before, "Compose table load (thread)", NULL);
}

static void
init_compose_table_async (GCancellable         *cancellable,
                          GAsyncReadyCallback   callback,
                          gpointer              user_data)
{
  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, init_compose_table_async);
  g_task_run_in_thread (task, init_compose_table_thread_cb);
  g_object_unref (task);
}

static void
gtk_im_context_simple_init (GtkIMContextSimple *context_simple)
{
  GtkIMContextSimplePrivate *priv;

  priv = context_simple->priv = gtk_im_context_simple_get_instance_private (context_simple);

  priv->compose_buffer_len = builtin_compose_table.max_seq_len + 1;
  priv->compose_buffer = g_new0 (guint, priv->compose_buffer_len);
  priv->tentative_match = g_string_new ("");
  priv->tentative_match_len = 0;
}

static void
gtk_im_context_simple_finalize (GObject *obj)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (obj);
  GtkIMContextSimplePrivate *priv = context_simple->priv;;

  g_free (priv->compose_buffer);
  g_string_free (priv->tentative_match, TRUE);

  G_OBJECT_CLASS (gtk_im_context_simple_parent_class)->finalize (obj);
}

/**
 * gtk_im_context_simple_new:
 *
 * Creates a new `GtkIMContextSimple`.
 *
 * Returns: a new `GtkIMContextSimple`
 */
GtkIMContext *
gtk_im_context_simple_new (void)
{
  return g_object_new (GTK_TYPE_IM_CONTEXT_SIMPLE, NULL);
}

static void
gtk_im_context_simple_commit_string (GtkIMContextSimple *context_simple,
                                     const char         *str)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;

  if (priv->in_hex_sequence ||
      priv->tentative_match_len > 0 ||
      priv->compose_buffer[0] != 0)
    {
      g_string_set_size (priv->tentative_match, 0);
      priv->tentative_match_len = 0;
      priv->in_hex_sequence = FALSE;
      priv->in_compose_sequence = FALSE;
      priv->compose_buffer[0] = 0;

      g_signal_emit_by_name (context_simple, "preedit-changed");
      g_signal_emit_by_name (context_simple, "preedit-end");
    }

  g_signal_emit_by_name (context_simple, "commit", str);
}

static void
gtk_im_context_simple_commit_char (GtkIMContextSimple *context_simple,
                                   gunichar            ch)
{
  char buf[8] = { 0, };

  g_unichar_to_utf8 (ch, buf);

  gtk_im_context_simple_commit_string (context_simple, buf);
}

/* In addition to the table-driven sequences, we allow Unicode hex
 * codes to be entered. The method chosen here is similar to the
 * one recommended in ISO 14755, but not exactly the same, since we
 * don’t want to steal 16 valuable key combinations.
 *
 * A hex Unicode sequence must be started with Ctrl-Shift-U, followed
 * by a sequence of hex digits entered with Ctrl-Shift still held.
 * Releasing one of the modifiers or pressing space while the modifiers
 * are still held commits the character. It is possible to erase
 * digits using backspace.
 *
 * As an extension to the above, we also allow to start the sequence
 * with Ctrl-Shift-U, then release the modifiers before typing any
 * digits, and enter the digits without modifiers.
 */

static gboolean
check_hex (GtkIMContextSimple *context_simple,
           int                 n_compose)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  /* See if this is a hex sequence, return TRUE if so */
  int i;
  GString *str;
  gulong n;
  char *nptr = NULL;
  char buf[7];

  g_string_set_size (priv->tentative_match, 0);
  priv->tentative_match_len = 0;

  str = g_string_new (NULL);

  i = 0;
  while (i < n_compose)
    {
      gunichar ch;

      ch = gdk_keyval_to_unicode (priv->compose_buffer[i]);

      if (ch == 0 || !g_unichar_isxdigit (ch))
        {
          g_string_free (str, TRUE);
          return FALSE;
        }

      buf[g_unichar_to_utf8 (ch, buf)] = '\0';

      g_string_append (str, buf);

      ++i;
    }

  n = strtoul (str->str, &nptr, 16);

  /* If strtoul fails it probably means non-latin digits were used;
   * we should in principle handle that, but we probably don't.
   */
  if (nptr - str->str < str->len)
    {
      g_string_free (str, TRUE);
      return FALSE;
    }
  else
    g_string_free (str, TRUE);

  if (g_unichar_validate (n))
    {
      g_string_set_size (priv->tentative_match, 0);
      g_string_append_unichar (priv->tentative_match, n);
      priv->tentative_match_len = n_compose;
    }

  return TRUE;
}

static void
beep_surface (GdkSurface *surface)
{
  GdkDisplay *display = gdk_surface_get_display (surface);
  gboolean   beep;

  g_object_get (gtk_settings_get_for_display (display),
                "gtk-error-bell", &beep,
                NULL);

  if (beep)
    gdk_surface_beep (surface);
}

static inline gboolean
is_dead_key (guint keysym)
{
  return GDK_KEY_dead_grave <= keysym && keysym <= GDK_KEY_dead_greek;
}

static void
append_dead_key (GString *string,
                 guint    keysym)
{
  /* Sadly, not all the dead keysyms have spacing mark equivalents
   * in Unicode. For those that don't, we use NBSP + the non-spacing
   * mark as an approximation.
   */
  switch (keysym)
    {
#define CASE(keysym, unicode, sp)                \
    case GDK_KEY_dead_##keysym:                  \
      if (sp)                                    \
        g_string_append_unichar (string, 0xA0);  \
      g_string_append_unichar (string, unicode); \
      break;

    CASE (grave, 0x60, 0);
    CASE (acute, 0xb4, 0);
    CASE (circumflex, 0x5e, 0);
    CASE (tilde, 0x7e, 0);
    CASE (macron, 0xaf, 0);
    CASE (breve, 0x2d8, 0);
    CASE (abovedot, 0x307, 1);
    CASE (diaeresis, 0xa8, 0);
    CASE (abovering, 0x2da, 0);
    CASE (hook, 0x2c0, 0);
    CASE (doubleacute, 0x2dd, 0);
    CASE (caron, 0x2c7, 0);
    CASE (cedilla, 0xb8, 0);
    CASE (ogonek, 0x2db, 0);
    CASE (iota, 0x37a, 0);
    CASE (voiced_sound, 0x3099, 1);
    CASE (semivoiced_sound, 0x309a, 1);
    CASE (belowdot, 0x323, 1);
    CASE (horn, 0x31b, 1);
    CASE (stroke, 0x335, 1);
    CASE (abovecomma, 0x2bc, 0);
    CASE (abovereversedcomma, 0x2bd, 1);
    CASE (doublegrave, 0x30f, 1);
    CASE (belowring, 0x2f3, 0);
    CASE (belowmacron, 0x2cd, 0);
    CASE (belowcircumflex, 0x32d, 1);
    CASE (belowtilde, 0x330, 1);
    CASE (belowbreve, 0x32e, 1);
    CASE (belowdiaeresis, 0x324, 1);
    CASE (invertedbreve, 0x32f, 1);
    CASE (belowcomma, 0x326, 1);
    CASE (lowline, 0x5f, 0);
    CASE (aboveverticalline, 0x2c8, 0);
    CASE (belowverticalline, 0x2cc, 0);
    CASE (longsolidusoverlay, 0x338, 1);
    CASE (a, 0x363, 1);
    CASE (A, 0x363, 1);
    CASE (e, 0x364, 1);
    CASE (E, 0x364, 1);
    CASE (i, 0x365, 1);
    CASE (I, 0x365, 1);
    CASE (o, 0x366, 1);
    CASE (O, 0x366, 1);
    CASE (u, 0x367, 1);
    CASE (U, 0x367, 1);
    CASE (small_schwa, 0x1dea, 1);
    CASE (capital_schwa, 0x1dea, 1);
#undef CASE
    default:
      g_string_append_unichar (string, gdk_keyval_to_unicode (keysym));
    }
}

static gboolean
no_sequence_matches (GtkIMContextSimple *context_simple,
                     int                 n_compose,
                     GdkEvent           *event)
{
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  GtkIMContext *context;
  gunichar ch;
  guint keyval;

  context = GTK_IM_CONTEXT (context_simple);

  priv->in_compose_sequence = FALSE;

  /* No compose sequences found, check first if we have a partial
   * match pending.
   */
  if (priv->tentative_match_len > 0)
    {
      int len = priv->tentative_match_len;
      int i;
      guint *compose_buffer;
      char *str;

      compose_buffer = alloca (sizeof (guint) * priv->compose_buffer_len);

      memcpy (compose_buffer, priv->compose_buffer, sizeof (guint) * priv->compose_buffer_len);

      str = g_strdup (priv->tentative_match->str);
      gtk_im_context_simple_commit_string (context_simple, str);
      g_free (str);

      for (i = 0; i < n_compose - len - 1; i++)
        {
          GdkTranslatedKey translated;
          translated.keyval = compose_buffer[len + i];
          translated.consumed = 0;
          translated.layout = 0;
          translated.level = 0;
          GdkEvent *tmp_event = gdk_key_event_new (GDK_KEY_PRESS,
                                                   gdk_event_get_surface (event),
                                                   gdk_event_get_device (event),
                                                   gdk_event_get_time (event),
                                                   compose_buffer[len + i],
                                                   gdk_event_get_modifier_state (event),
                                                   FALSE,
                                                   &translated,
                                                   &translated,
                                                   NULL);

	  gtk_im_context_filter_keypress (context, tmp_event);
	  gdk_event_unref (tmp_event);
	}

      return gtk_im_context_filter_keypress (context, event);
    }
  else
    {
      int i;

      for (i = 0; i < n_compose && is_dead_key (priv->compose_buffer[i]); i++)
        ;

      if (n_compose > 1 && i >= n_compose - 1)
        {
          GString *s;

          s = g_string_new ("");

          if (i == n_compose - 1)
            {
              /* dead keys are never *really* dead */
              for (int j = 0; j < i; j++)
                append_dead_key (s, priv->compose_buffer[j]);

              ch = gdk_keyval_to_unicode (priv->compose_buffer[i]);
              if (ch != 0 && ch != ' ' && !g_unichar_iscntrl (ch))
                g_string_append_unichar (s, ch);

              gtk_im_context_simple_commit_string (context_simple, s->str);
            }
          else
            {
              append_dead_key (s, priv->compose_buffer[0]);
              gtk_im_context_simple_commit_string (context_simple, s->str);

              for (i = 1; i < n_compose; i++)
                priv->compose_buffer[i - 1] = priv->compose_buffer[i];
              priv->compose_buffer[n_compose - 1] = 0;

              priv->in_compose_sequence = TRUE;

              g_signal_emit_by_name (context, "preedit-start");
              g_signal_emit_by_name (context, "preedit-changed");
            }

          g_string_free (s, TRUE);

          return TRUE;
        }

      priv->compose_buffer[0] = 0;
      if (n_compose > 1)		/* Invalid sequence */
	{
	  beep_surface (gdk_event_get_surface (event));
          g_signal_emit_by_name (context, "preedit-changed");
          g_signal_emit_by_name (context, "preedit-end");
	  return TRUE;
	}

      keyval = gdk_key_event_get_keyval (event);
      ch = gdk_keyval_to_unicode (keyval);
      if (ch != 0 && !g_unichar_iscntrl (ch))
	{
	  gtk_im_context_simple_commit_char (context_simple, ch);
	  return TRUE;
	}
      else
	return FALSE;
    }
  return FALSE;
}

static gboolean
is_hex_keyval (guint keyval)
{
  gunichar ch = gdk_keyval_to_unicode (keyval);

  return g_unichar_isxdigit (ch);
}

static guint
canonical_hex_keyval (GdkEvent *event)
{
  guint keyval, event_keyval;
  guint *keyvals = NULL;
  int n_vals = 0;
  int i;

  event_keyval = gdk_key_event_get_keyval (event);

  /* See if the keyval is already a hex digit */
  if (is_hex_keyval (event_keyval))
    return event_keyval;

  /* See if this key would have generated a hex keyval in
   * any other state, and return that hex keyval if so
   */
  gdk_display_map_keycode (gdk_event_get_display (event),
                           gdk_key_event_get_keycode (event),
                           NULL,
                           &keyvals, &n_vals);

  keyval = 0;
  i = 0;
  while (i < n_vals)
    {
      if (is_hex_keyval (keyvals[i]))
        {
          keyval = keyvals[i];
          break;
        }

      ++i;
    }

  g_free (keyvals);

  if (keyval)
    return keyval;
  else
    /* No way to make it a hex digit
     */
    return 0;
}

static gboolean
gtk_im_context_simple_filter_keypress (GtkIMContext *context,
				       GdkEvent     *event)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  GdkSurface *surface = gdk_event_get_surface ((GdkEvent *) event);
  GSList *tmp_list;
  int n_compose = 0;
  GdkModifierType hex_mod_mask;
  gboolean have_hex_mods;
  gboolean is_hex_start;
  gboolean is_hex_end;
  gboolean is_backspace;
  gboolean is_escape;
  guint hex_keyval;
  int i;
  gboolean compose_finish;
  gboolean compose_match;
  guint keyval, state;

  while (n_compose < priv->compose_buffer_len && priv->compose_buffer[n_compose] != 0)
    n_compose++;

  keyval = gdk_key_event_get_keyval (event);
  state = gdk_event_get_modifier_state (event);

  if (gdk_event_get_event_type (event) == GDK_KEY_RELEASE)
    {
      if (priv->in_hex_sequence &&
          (keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R ||
	   keyval == GDK_KEY_Shift_L || keyval == GDK_KEY_Shift_R))
	{
          if (priv->tentative_match->len > 0)
	    {
              char *str = g_strdup (priv->tentative_match->str);
	      gtk_im_context_simple_commit_string (context_simple, str);
              g_free (str);

	      return TRUE;
	    }
	  else if (n_compose == 0)
	    {
	      priv->modifiers_dropped = TRUE;

	      return TRUE;
	    }
	  else if (priv->in_hex_sequence)
	    {
	      /* invalid hex sequence */
	      beep_surface (surface);

              g_string_set_size (priv->tentative_match, 0);
	      priv->in_hex_sequence = FALSE;
	      priv->compose_buffer[0] = 0;

	      g_signal_emit_by_name (context_simple, "preedit-changed");
	      g_signal_emit_by_name (context_simple, "preedit-end");

	      return TRUE;
	    }
	}

      if (priv->in_hex_sequence || priv->in_compose_sequence)
        return TRUE; /* Don't leak random key events during preedit */

      return FALSE;
    }

  /* Ignore modifier key presses */
  for (i = 0; i < G_N_ELEMENTS (gtk_compose_ignore); i++)
    if (keyval == gtk_compose_ignore[i])
      {
        if (priv->in_hex_sequence || priv->in_compose_sequence)
          return TRUE; /* Don't leak random key events during preedit */

        return FALSE;
      }

  hex_mod_mask = GDK_CONTROL_MASK|GDK_SHIFT_MASK;

  if (priv->in_hex_sequence && priv->modifiers_dropped)
    have_hex_mods = TRUE;
  else
    have_hex_mods = (state & (hex_mod_mask)) == hex_mod_mask;
  is_hex_start = keyval == GDK_KEY_U;
  is_hex_end = (keyval == GDK_KEY_space ||
                keyval == GDK_KEY_KP_Space ||
                keyval == GDK_KEY_Return ||
                keyval == GDK_KEY_ISO_Enter ||
                keyval == GDK_KEY_KP_Enter);
  is_backspace = keyval == GDK_KEY_BackSpace;
  is_escape = keyval == GDK_KEY_Escape;
  hex_keyval = canonical_hex_keyval (event);

  /* If we are already in a non-hex sequence, or
   * this keystroke is not hex modifiers + hex digit, don't filter
   * key events with accelerator modifiers held down. We only treat
   * Control and Alt as accel modifiers here, since Super, Hyper and
   * Meta are often co-located with Mode_Switch, Multi_Key or
   * ISO_Level3_Switch.
   */
  if (!have_hex_mods ||
      (n_compose > 0 && !priv->in_hex_sequence) ||
      (n_compose == 0 && !priv->in_hex_sequence && !is_hex_start) ||
      (priv->in_hex_sequence && !hex_keyval &&
       !is_hex_start && !is_hex_end && !is_escape && !is_backspace))
    {
      GdkModifierType no_text_input_mask;
      GdkModifierType consumed_modifiers = 0;

      no_text_input_mask = GDK_ALT_MASK|GDK_CONTROL_MASK;

#ifdef G_OS_WIN32
     /* On Win32, even Ctrl + Alt could be text input because AltGr = Ctrl
      * + Alt. For example, Ctrl + Alt + e = € on a German keyboard. The
      * GdkEvent's state, however, reports *all* modifiers that were
      * active at the time the key was pressed, including the ones that
      * were consumed to generate the keyval. So we cannot just assume
      * that any key event containing Ctrl or Alt is a keybinding. We have
      * to first check if those modifiers were actually used to generate
      * the keyval. If so, then the keypress is regular input and we
      * should not exit here.
      */
      consumed_modifiers = gdk_key_event_get_consumed_modifiers (event);
#endif

      if (priv->in_hex_sequence && priv->modifiers_dropped &&
	  (keyval == GDK_KEY_Return ||
	   keyval == GDK_KEY_ISO_Enter ||
	   keyval == GDK_KEY_KP_Enter))
	{
	  return FALSE;
	}

      if (state & no_text_input_mask & ~consumed_modifiers)
        {
          if (priv->in_hex_sequence || priv->in_compose_sequence)
            return TRUE; /* Don't leak random key events during preedit */

          return FALSE;
        }
    }

  /* Handle backspace */
  if (priv->in_hex_sequence && have_hex_mods && is_backspace)
    {
      if (n_compose > 0)
	{
	  n_compose--;
	  priv->compose_buffer[n_compose] = 0;
          check_hex (context_simple, n_compose);
	}
      else
	{
	  priv->in_hex_sequence = FALSE;
	}

      g_signal_emit_by_name (context_simple, "preedit-changed");

      if (!priv->in_hex_sequence)
        g_signal_emit_by_name (context_simple, "preedit-end");

      return TRUE;
    }

  if (!priv->in_hex_sequence && n_compose > 0 && is_backspace)
    {
      n_compose--;
      priv->compose_buffer[n_compose] = 0;

      g_signal_emit_by_name (context_simple, "preedit-changed");

      if (n_compose == 0)
        g_signal_emit_by_name (context_simple, "preedit-end");

      return TRUE;
    }

  /* Check for hex sequence restart */
  if (priv->in_hex_sequence && have_hex_mods && is_hex_start)
    {
      if (priv->tentative_match->len > 0)
	{
          char *str = g_strdup (priv->tentative_match->str);
	  gtk_im_context_simple_commit_string (context_simple, str);
          g_free (str);
	}
      else
	{
	  /* invalid hex sequence */
	  if (n_compose > 0)
	    beep_surface (surface);

          g_string_set_size (priv->tentative_match, 0);
	  priv->in_hex_sequence = FALSE;
	  priv->compose_buffer[0] = 0;
	}
    }

  /* Check for hex sequence start */
  if (!priv->in_hex_sequence && have_hex_mods && is_hex_start)
    {
      priv->compose_buffer[0] = 0;
      priv->in_hex_sequence = TRUE;
      priv->modifiers_dropped = FALSE;
      g_string_set_size (priv->tentative_match, 0);

      g_signal_emit_by_name (context_simple, "preedit-start");
      g_signal_emit_by_name (context_simple, "preedit-changed");

      return TRUE;
    }

  if (is_escape)
    {
      if (priv->in_hex_sequence || priv->in_compose_sequence)
        {
	  gtk_im_context_simple_reset (context);
	  return TRUE;
        }

      return FALSE;
    }

  if (priv->in_hex_sequence)
    {
      if (hex_keyval && n_compose < 6)
	priv->compose_buffer[n_compose++] = hex_keyval;
      else if (!is_hex_end)
	{
	  /* non-hex character in hex sequence, or sequence too long */
	  beep_surface (surface);
	  return TRUE;
	}
    }
  else
    {
      if (n_compose + 1 == priv->compose_buffer_len)
        {
          priv->compose_buffer_len += 1;
          priv->compose_buffer = g_renew (guint, priv->compose_buffer, priv->compose_buffer_len);
        }

      priv->compose_buffer[n_compose++] = keyval;
    }

  priv->compose_buffer[n_compose] = 0;

  if (priv->in_hex_sequence)
    {
      /* If the modifiers are still held down, consider the sequence again */
      if (have_hex_mods)
        {
          /* space or return ends the sequence, and we eat the key */
          if (n_compose > 0 && is_hex_end)
            {
	      if (priv->tentative_match->len > 0)
		{
                  char *str = g_strdup (priv->tentative_match->str);
		  gtk_im_context_simple_commit_string (context_simple, str);
                  g_free (str);

                  return TRUE;
		}
	      else
		{
		  /* invalid hex sequence */
		  beep_surface (surface);

                  g_string_set_size (priv->tentative_match, 0);
		  priv->in_hex_sequence = FALSE;
		  priv->compose_buffer[0] = 0;
		}
            }
          else if (!check_hex (context_simple, n_compose))
	    beep_surface (surface);

	  g_signal_emit_by_name (context_simple, "preedit-changed");

	  if (!priv->in_hex_sequence)
	    g_signal_emit_by_name (context_simple, "preedit-end");

	  return TRUE;
        }
    }
  else /* Then, check for compose sequences */
    {
      gboolean success = FALSE;
      int prefix = 0;
      GString *output;

      output = g_string_new ("");

      G_LOCK (global_tables);

      tmp_list = global_tables;
      while (tmp_list)
        {
          if (gtk_compose_table_check ((GtkComposeTable *)tmp_list->data,
                                       priv->compose_buffer, n_compose,
                                       &compose_finish, &compose_match,
                                       output))
            {
              if (!priv->in_compose_sequence)
                {
                  priv->in_compose_sequence = TRUE;
                  g_signal_emit_by_name (context_simple, "preedit-start");
                }

              if (compose_finish)
                {
                  if (compose_match)
                    gtk_im_context_simple_commit_string (context_simple, output->str);
                }
              else
                {
                  if (compose_match)
                    {
                      g_string_assign (priv->tentative_match, output->str);
                      priv->tentative_match_len = n_compose;
                    }
                  g_signal_emit_by_name (context_simple, "preedit-changed");
                }

              success = TRUE;
              break;
            }
          else
            {
              int table_prefix;

              gtk_compose_table_get_prefix ((GtkComposeTable *)tmp_list->data,
                                            priv->compose_buffer, n_compose,
                                            &table_prefix);

              prefix = MAX (prefix, table_prefix);
            }

          tmp_list = tmp_list->next;
        }

      G_UNLOCK (global_tables);

      if (success)
        {
          g_string_free (output, TRUE);
          return TRUE;
        }

      if (gtk_check_algorithmically (priv->compose_buffer, n_compose, output))
        {
          if (!priv->in_compose_sequence)
            {
              priv->in_compose_sequence = TRUE;
              g_signal_emit_by_name (context_simple, "preedit-start");
            }

          if (output->len > 0)
            gtk_im_context_simple_commit_string (context_simple, output->str);
          else
            g_signal_emit_by_name (context_simple, "preedit-changed");

          g_string_free (output, TRUE);

          return TRUE;
        }

      g_string_free (output, TRUE);

      /* If we get here, no Compose sequence matched.
       * Only beep if we were in a sequence before.
       */
      if (prefix > 0)
        {
          for (i = prefix; i < n_compose; i++)
            priv->compose_buffer[i] = 0;

          beep_surface (gdk_event_get_surface (event));

          g_signal_emit_by_name (context_simple, "preedit-changed");

          return TRUE;
        }
    }

  /* The current compose_buffer doesn't match anything */
  return no_sequence_matches (context_simple, n_compose, (GdkEvent *)event);
}

static void
gtk_im_context_simple_reset (GtkIMContext *context)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  GtkIMContextSimplePrivate *priv = context_simple->priv;

  priv->compose_buffer[0] = 0;

  if (priv->tentative_match->len > 0 ||
      priv->in_hex_sequence ||
      priv->in_compose_sequence)
    {
      priv->in_hex_sequence = FALSE;
      priv->in_compose_sequence = FALSE;
      g_string_set_size (priv->tentative_match, 0);
      priv->tentative_match_len = 0;
      g_signal_emit_by_name (context_simple, "preedit-changed");
      g_signal_emit_by_name (context_simple, "preedit-end");
    }
}

static void
gtk_im_context_simple_get_preedit_string (GtkIMContext   *context,
                                          char          **str,
                                          PangoAttrList **attrs,
                                          int            *cursor_pos)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  GtkIMContextSimplePrivate *priv = context_simple->priv;
  GString *s;
  int i;

  s = g_string_new ("");

  if (priv->in_hex_sequence)
    {
      g_string_append_c (s, 'u');

      for (i = 0; priv->compose_buffer[i]; i++)
        g_string_append_unichar (s, gdk_keyval_to_unicode (priv->compose_buffer[i]));
    }
  else if (priv->in_compose_sequence)
    {
      if (priv->tentative_match_len > 0 && priv->compose_buffer[0] != 0)
        {
          g_string_append (s, priv->tentative_match->str);
        }
      else
        {
          for (i = 0; priv->compose_buffer[i]; i++)
            {
              if (priv->compose_buffer[i] == GDK_KEY_Multi_key)
                {
                  /* We only show the Compose key visibly when it is the
                   * only glyph in the preedit, or when the sequence contains
                   * multiple Compose keys, or when it occurs in the
                   * middle of the sequence. Sadly, the official character,
                   * U+2384, COMPOSITION SYMBOL, is bit too distracting, so
                   * we use U+00B7, MIDDLE DOT.
                   */
                  if (priv->compose_buffer[1] == 0 || i > 0 ||
                      priv->compose_buffer[i + 1] == GDK_KEY_Multi_key)
                    g_string_append (s, "·");
                }
              else
                {
                  gunichar ch;

                  if (is_dead_key (priv->compose_buffer[i]))
                    {
                      append_dead_key (s, priv->compose_buffer[i]);
                    }
                  else
                    {
                      ch = gdk_keyval_to_unicode (priv->compose_buffer[i]);
                      if (ch)
                        g_string_append_unichar (s, ch);
                    }
                }
            }
        }
    }

  if (cursor_pos)
    *cursor_pos = g_utf8_strlen (s->str, s->len);

  if (attrs)
    {
      *attrs = pango_attr_list_new ();

      if (s->len)
        {
          PangoAttribute *attr;

          attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
          attr->start_index = 0;
          attr->end_index = s->len;
          pango_attr_list_insert (*attrs, attr);

          attr = pango_attr_fallback_new (TRUE);
          attr->start_index = 0;
          attr->end_index = s->len;
          pango_attr_list_insert (*attrs, attr);
        }
    }

  if (str)
    *str = g_string_free (s, FALSE);
}

/**
 * gtk_im_context_simple_add_table: (skip)
 * @context_simple: A `GtkIMContextSimple`
 * @data: (array): the table
 * @max_seq_len: Maximum length of a sequence in the table
 * @n_seqs: number of sequences in the table
 *
 * Adds an additional table to search to the input context.
 * Each row of the table consists of @max_seq_len key symbols
 * followed by two #guint16 interpreted as the high and low
 * words of a #gunicode value. Tables are searched starting
 * from the last added.
 *
 * The table must be sorted in dictionary order on the
 * numeric value of the key symbol fields. (Values beyond
 * the length of the sequence should be zero.)
 *
 * Deprecated: 4.4: Use gtk_im_context_simple_add_compose_file()
 */
void
gtk_im_context_simple_add_table (GtkIMContextSimple *context_simple,
                                 guint16            *data,
                                 int                 max_seq_len,
                                 int                 n_seqs)
{
  g_return_if_fail (GTK_IS_IM_CONTEXT_SIMPLE (context_simple));

  add_compose_table_from_data (data, max_seq_len, n_seqs);
}

/**
 * gtk_im_context_simple_add_compose_file:
 * @context_simple: A `GtkIMContextSimple`
 * @compose_file: The path of compose file
 *
 * Adds an additional table from the X11 compose file.
 */
void
gtk_im_context_simple_add_compose_file (GtkIMContextSimple *context_simple,
                                        const char         *compose_file)
{
  g_return_if_fail (GTK_IS_IM_CONTEXT_SIMPLE (context_simple));

  add_compose_table_from_file (compose_file);
}
