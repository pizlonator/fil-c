/*
 * Copyright (C) 2018 Canonical Ltd.
 * Copyright (C) 2023 Marco Melorio
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gsound.h>

#include "config.h"
#include "cc-alert-chooser-window.h"

#define KEY_SOUNDS_SCHEMA "org.gnome.desktop.sound"

struct _CcAlertChooserWindow
{
  AdwWindow       parent_instance;

  GtkCheckButton *none_button;
  GtkCheckButton *click_button;
  GtkCheckButton *string_button;
  GtkCheckButton *swing_button;
  GtkCheckButton *hum_button;

  GSoundContext  *context;
  GSettings      *sound_settings;
};

G_DEFINE_TYPE (CcAlertChooserWindow, cc_alert_chooser_window, ADW_TYPE_WINDOW)

#define CUSTOM_THEME_NAME "__custom"

static gchar *
get_theme_dir (void)
{
  return g_build_filename (g_get_user_data_dir (), "sounds", CUSTOM_THEME_NAME, NULL);
}

static gchar *
get_sound_path (const gchar *name)
{
  g_autofree gchar *filename = NULL;

  filename = g_strdup_printf ("%s.ogg", name);
  return g_build_filename (SOUND_DATA_DIR, "gnome", "default", "alerts", filename, NULL);
}

static gchar *
get_alert_name (void)
{
  g_autofree gchar *dir = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFileInfo) info = NULL;
  const gchar *target;
  g_autofree gchar *basename = NULL;
  g_autoptr(GError) error = NULL;

  dir = get_theme_dir ();
  path = g_build_filename (dir, "bell-terminal.ogg", NULL);
  file = g_file_new_for_path (path);

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
  if (info == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_warning ("Failed to get sound theme symlink %s: %s", path, error->message);
      return NULL;
    }
  target = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);
  if (target == NULL)
    return NULL;

  basename = g_path_get_basename (target);
  if (g_str_has_suffix (basename, ".ogg"))
    basename[strlen (basename) - 4] = '\0';

  return g_steal_pointer (&basename);
}

static void
set_sound_symlink (const gchar *alert_name,
                   const gchar *name)
{
  g_autofree gchar *dir = NULL;
  g_autofree gchar *source_filename = NULL;
  g_autofree gchar *source_path = NULL;
  g_autofree gchar *target_path = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;

  dir = get_theme_dir ();
  source_filename = g_strdup_printf ("%s.ogg", alert_name);
  source_path = g_build_filename (dir, source_filename, NULL);
  target_path = get_sound_path (name);

  file = g_file_new_for_path (source_path);
  if (!g_file_delete (file, NULL, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_warning ("Failed to remove existing sound symbolic link %s: %s", source_path, error->message);
    }
  if (!g_file_make_symbolic_link (file, target_path, NULL, &error))
    g_warning ("Failed to make sound theme symbolic link %s->%s: %s", source_path, target_path, error->message);
}

static void
update_dir_mtime (const char *dir_path)
{
  g_autoptr(GFile) dir = NULL;
  g_autoptr(GDateTime) now = NULL;
  g_autoptr(GError) mtime_error = NULL;

  now = g_date_time_new_now_utc ();
  dir = g_file_new_for_path (dir_path);
  if (!g_file_set_attribute_uint64 (dir,
                                    G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                    g_date_time_to_unix (now),
                                    G_FILE_QUERY_INFO_NONE,
                                    NULL,
                                    &mtime_error))
    {
      g_warning ("Failed to update directory modification time for %s: %s",
                 dir_path, mtime_error->message);
    }
}

static void
set_custom_theme (CcAlertChooserWindow *self,
                  const gchar          *name)
{
  g_autofree gchar *dir_path = NULL;
  g_autofree gchar *theme_path = NULL;
  g_autofree gchar *sounds_path = NULL;
  g_autoptr(GKeyFile) theme_file = NULL;
  g_autoptr(GVariant) default_theme = NULL;
  g_autoptr(GError) load_error = NULL;
  g_autoptr(GError) save_error = NULL;

  dir_path = get_theme_dir ();
  g_mkdir_with_parents (dir_path, USER_DIR_MODE);

  theme_path = g_build_filename (dir_path, "index.theme", NULL);

  default_theme = g_settings_get_default_value (self->sound_settings, "theme-name");

  theme_file = g_key_file_new ();
  if (!g_key_file_load_from_file (theme_file, theme_path, G_KEY_FILE_KEEP_COMMENTS, &load_error))
    {
      if (!g_error_matches (load_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_printerr ("Failed to load theme file %s: %s", theme_path, load_error->message);
    }
  g_key_file_set_string (theme_file, "Sound Theme", "Name", _("Custom"));
  if (default_theme != NULL)
    g_key_file_set_string (theme_file, "Sound Theme", "Inherits", g_variant_get_string (default_theme, NULL));
  g_key_file_set_string (theme_file, "Sound Theme", "Directories", ".");

  if (!g_key_file_save_to_file (theme_file, theme_path, &save_error))
    {
      g_warning ("Failed to save theme file %s: %s", theme_path, save_error->message);
    }

  set_sound_symlink ("bell-terminal", name);
  set_sound_symlink ("bell-window-system", name);

  /* Ensure canberra's event-sound-cache will get updated when g-s-d
   * clears the cached samples.
   */
  sounds_path = g_build_filename (g_get_user_data_dir (), "sounds", NULL);
  update_dir_mtime (sounds_path);

  /* Ensure the g-s-d sound plugin which does non-recursive monitoring
   * notices the change even if the theme directory already existed.
   */
  update_dir_mtime (dir_path);

  g_settings_set_boolean (self->sound_settings, "event-sounds", TRUE);
  g_settings_set_string (self->sound_settings, "theme-name", CUSTOM_THEME_NAME);
}

static void
play_sound (CcAlertChooserWindow *self,
            const gchar          *name)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GError) error = NULL;

  path = get_sound_path (name);
  if (!gsound_context_play_simple (self->context, NULL, &error,
                                   GSOUND_ATTR_MEDIA_FILENAME, path,
                                   NULL))
    {
      g_warning ("Failed to play alert sound %s: %s", path, error->message);
    }
}

static void
activate_cb (CcAlertChooserWindow *self)
{
  if (gtk_check_button_get_active (self->click_button))
    play_sound (self, "click");
  else if (gtk_check_button_get_active (self->string_button))
    play_sound (self, "string");
  else if (gtk_check_button_get_active (self->swing_button))
    play_sound (self, "swing");
  else if (gtk_check_button_get_active (self->hum_button))
    play_sound (self, "hum");
}

static void
toggled_cb (CcAlertChooserWindow *self)
{
  if (gtk_check_button_get_active (self->none_button))
    g_settings_set_boolean (self->sound_settings, "event-sounds", FALSE);
  else if (gtk_check_button_get_active (self->click_button))
    set_custom_theme (self, "click");
  else if (gtk_check_button_get_active (self->string_button))
    set_custom_theme (self, "string");
  else if (gtk_check_button_get_active (self->swing_button))
    set_custom_theme (self, "swing");
  else if (gtk_check_button_get_active (self->hum_button))
    set_custom_theme (self, "hum");
}

static void
set_button_active (CcAlertChooserWindow *self,
                   GtkCheckButton       *button,
                   gboolean              active)
{
  g_signal_handlers_block_by_func (button, toggled_cb, self);
  gtk_check_button_set_active (button, active);
  g_signal_handlers_unblock_by_func (button, toggled_cb, self);
}

static void
cc_alert_chooser_window_dispose (GObject *object)
{
  CcAlertChooserWindow *self = CC_ALERT_CHOOSER_WINDOW (object);

  g_clear_object (&self->context);
  g_clear_object (&self->sound_settings);

  G_OBJECT_CLASS (cc_alert_chooser_window_parent_class)->dispose (object);
}

void
cc_alert_chooser_window_class_init (CcAlertChooserWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_alert_chooser_window_dispose;

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-alert-chooser-window.ui");

  gtk_widget_class_bind_template_child (widget_class, CcAlertChooserWindow, none_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooserWindow, click_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooserWindow, string_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooserWindow, swing_button);
  gtk_widget_class_bind_template_child (widget_class, CcAlertChooserWindow, hum_button);

  gtk_widget_class_bind_template_callback (widget_class, activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, toggled_cb);
}

void
cc_alert_chooser_window_init (CcAlertChooserWindow *self)
{
  g_autofree gchar *alert_name = NULL;
  g_autoptr(GError) error = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->context = gsound_context_new (NULL, &error);
  if (self->context == NULL)
    g_error ("Failed to make sound context: %s", error->message);

  self->sound_settings = g_settings_new (KEY_SOUNDS_SCHEMA);

  alert_name = get_alert_name ();

  /* If user has selected an old sound alert, migrate them to click. */
  if (g_strcmp0 (alert_name, "click") != 0 &&
      g_strcmp0 (alert_name, "hum") != 0 &&
      g_strcmp0 (alert_name, "string") != 0 &&
      g_strcmp0 (alert_name, "swing") != 0)
    {
      set_custom_theme (self, "click");
      g_free (alert_name);
      alert_name = g_strdup ("click");
    }

  if (!g_settings_get_boolean (self->sound_settings, "event-sounds"))
    set_button_active (self, self->none_button, TRUE);
  else if (g_strcmp0 (alert_name, "click") == 0)
    set_button_active (self, self->click_button, TRUE);
  else if (g_strcmp0 (alert_name, "hum") == 0)
    set_button_active (self, self->hum_button, TRUE);
  else if (g_strcmp0 (alert_name, "string") == 0)
    set_button_active (self, self->string_button, TRUE);
  else if (g_strcmp0 (alert_name, "swing") == 0)
    set_button_active (self, self->swing_button, TRUE);
  else if (alert_name != NULL)
    g_warning ("Current alert sound has unknown name %s", alert_name);
}

CcAlertChooserWindow *
cc_alert_chooser_window_new (void)
{
  return g_object_new (CC_TYPE_ALERT_CHOOSER_WINDOW, NULL);
}

const gchar *
get_selected_alert_display_name (void)
{
  g_autofree gchar *alert_name = NULL;
  g_autoptr(GSettings) sound_settings = NULL;

  sound_settings = g_settings_new (KEY_SOUNDS_SCHEMA);

  alert_name = get_alert_name ();

  if (!g_settings_get_boolean (sound_settings, "event-sounds"))
    return _("None");
  else if (g_strcmp0 (alert_name, "click") == 0)
    return _("Click");
  else if (g_strcmp0 (alert_name, "hum") == 0)
    return _("Hum");
  else if (g_strcmp0 (alert_name, "string") == 0)
    return _("String");
  else if (g_strcmp0 (alert_name, "swing") == 0)
    return _("Swing");
  else
    return _("Unknown");
}
