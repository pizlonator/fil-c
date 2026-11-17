/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <shell/cc-panel.h>

#include "cc-applications-panel.h"
#include "cc-removable-media-settings.h"

/* Autorun options */
#define PREF_MEDIA_AUTORUN_NEVER                "autorun-never"
#define PREF_MEDIA_AUTORUN_X_CONTENT_START_APP  "autorun-x-content-start-app"
#define PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE     "autorun-x-content-ignore"
#define PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER "autorun-x-content-open-folder"

#define CUSTOM_ITEM_ASK "cc-item-ask"
#define CUSTOM_ITEM_DO_NOTHING "cc-item-do-nothing"
#define CUSTOM_ITEM_OPEN_FOLDER "cc-item-open-folder"

#define MEDIA_HANDLING_SCHEMA "org.gnome.desktop.media-handling"

struct _CcRemovableMediaSettings
{
  AdwPreferencesGroup  parent;

  GtkWidget           *cd_audio_row;
  GtkWidget           *dvd_video_row;
  GtkWidget           *music_player_row;
  GtkWidget           *photos_row;
  GtkWidget           *software_row;
  GtkWidget           *other_media_row;

  GtkAppChooserButton *audio_cdda_chooser;
  GtkAppChooserButton *dcf_chooser;
  GtkAppChooserButton *music_player_chooser;
  AdwWindow           *other_type_dialog;
  AdwActionRow        *other_action_row;
  GtkBox              *other_action_box;
  GtkComboBox         *other_type_combo_box;
  GtkListStore        *other_type_list_store;
  GtkAppChooserButton *software_chooser;
  GtkAppChooserButton *video_dvd_chooser;

  GtkAppChooserButton *other_application_chooser;
  GSettings           *settings;
};


G_DEFINE_TYPE (CcRemovableMediaSettings, cc_removable_media_settings, ADW_TYPE_PREFERENCES_GROUP)

static char **
remove_elem_from_str_array (char       **v,
                            const char  *s)
{
  GPtrArray *array;
  guint idx;

  array = g_ptr_array_new ();

  for (idx = 0; v[idx] != NULL; idx++) {
    if (g_strcmp0 (v[idx], s) == 0) {
      continue;
    }

    g_ptr_array_add (array, v[idx]);
  }

  g_ptr_array_add (array, NULL);

  g_free (v);

  return (char **) g_ptr_array_free (array, FALSE);
}

static char **
add_elem_to_str_array (char       **v,
                       const char  *s)
{
  GPtrArray *array;
  guint idx;

  array = g_ptr_array_new ();

  for (idx = 0; v[idx] != NULL; idx++) {
    g_ptr_array_add (array, v[idx]);
  }

  g_ptr_array_add (array, g_strdup (s));
  g_ptr_array_add (array, NULL);

  g_free (v);

  return (char **) g_ptr_array_free (array, FALSE);
}

static void
autorun_get_preferences (CcRemovableMediaSettings *self,
                         const char               *x_content_type,
                         gboolean                 *pref_start_app,
                         gboolean                 *pref_ignore,
                         gboolean                 *pref_open_folder)
{
  g_auto(GStrv) x_content_start_app = NULL;
  g_auto(GStrv) x_content_ignore = NULL;
  g_auto(GStrv) x_content_open_folder = NULL;

  g_return_if_fail (pref_start_app != NULL);
  g_return_if_fail (pref_ignore != NULL);
  g_return_if_fail (pref_open_folder != NULL);

  *pref_start_app = FALSE;
  *pref_ignore = FALSE;
  *pref_open_folder = FALSE;
  x_content_start_app = g_settings_get_strv (self->settings,
                                             PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
  x_content_ignore = g_settings_get_strv (self->settings,
                                          PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
  x_content_open_folder = g_settings_get_strv (self->settings,
                                               PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);
  if (x_content_start_app != NULL) {
    *pref_start_app = g_strv_contains ((const gchar * const *) x_content_start_app, x_content_type);
  }
  if (x_content_ignore != NULL) {
    *pref_ignore = g_strv_contains ((const gchar * const *) x_content_ignore, x_content_type);
  }
  if (x_content_open_folder != NULL) {
    *pref_open_folder = g_strv_contains ((const gchar * const *) x_content_open_folder, x_content_type);
  }
}

static void
autorun_set_preferences (CcRemovableMediaSettings *self,
                         const char               *x_content_type,
                         gboolean                  pref_start_app,
                         gboolean                  pref_ignore,
                         gboolean                  pref_open_folder)
{
  g_auto(GStrv) x_content_start_app = NULL;
  g_auto(GStrv) x_content_ignore = NULL;
  g_auto(GStrv) x_content_open_folder = NULL;

  g_assert (x_content_type != NULL);

  x_content_start_app = g_settings_get_strv (self->settings,
                                             PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
  x_content_ignore = g_settings_get_strv (self->settings,
                                          PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
  x_content_open_folder = g_settings_get_strv (self->settings,
                                               PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);

  x_content_start_app = remove_elem_from_str_array (x_content_start_app, x_content_type);
  if (pref_start_app) {
    x_content_start_app = add_elem_to_str_array (x_content_start_app, x_content_type);
  }
  g_settings_set_strv (self->settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_START_APP, (const gchar * const*) x_content_start_app);

  x_content_ignore = remove_elem_from_str_array (x_content_ignore, x_content_type);
  if (pref_ignore) {
    x_content_ignore = add_elem_to_str_array (x_content_ignore, x_content_type);
  }
  g_settings_set_strv (self->settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE, (const gchar * const*) x_content_ignore);

  x_content_open_folder = remove_elem_from_str_array (x_content_open_folder, x_content_type);
  if (pref_open_folder) {
    x_content_open_folder = add_elem_to_str_array (x_content_open_folder, x_content_type);
  }
  g_settings_set_strv (self->settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER, (const gchar * const*) x_content_open_folder);

}

static void
on_custom_item_activated_cb (CcRemovableMediaSettings *self,
                             const gchar              *item,
                             GtkAppChooser            *app_chooser)
{
  g_autofree gchar *content_type = NULL;

  content_type = gtk_app_chooser_get_content_type (app_chooser);

  if (g_strcmp0 (item, CUSTOM_ITEM_ASK) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, FALSE, FALSE);
  } else if (g_strcmp0 (item, CUSTOM_ITEM_OPEN_FOLDER) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, FALSE, TRUE);
  } else if (g_strcmp0 (item, CUSTOM_ITEM_DO_NOTHING) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, TRUE, FALSE);
  }
}

static void
on_chooser_changed_cb (CcRemovableMediaSettings *self,
                       GtkAppChooser            *chooser)
{
  g_autoptr(GAppInfo) info = NULL;
  g_autofree gchar *content_type = NULL;

  info = gtk_app_chooser_get_app_info (chooser);

  if (info == NULL)
    return;

  content_type = gtk_app_chooser_get_content_type (chooser);
  autorun_set_preferences (self, content_type,
                           TRUE, FALSE, FALSE);
  g_app_info_set_as_default_for_type (info, content_type, NULL);
}

/* FIXME: Port away from GtkAppChooserButton entirely */
static void
ellipsize_app_chooser (GtkAppChooserButton *button)
{
  GtkWidget *child;
  g_autoptr (GList) cells = NULL;
  GtkCellRenderer *renderer;

  g_assert (GTK_IS_APP_CHOOSER_BUTTON (button));

  child = gtk_widget_get_first_child (GTK_WIDGET (button));

  g_assert (GTK_IS_CELL_LAYOUT (child));

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (child));

  g_assert (g_list_length (cells) > 0);

  renderer = g_list_last (cells)->data;

  g_assert (GTK_IS_CELL_RENDERER_TEXT (renderer));

  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
}

static void
prepare_chooser (CcRemovableMediaSettings *self,
                 GtkAppChooserButton      *button,
                 const gchar              *heading)
{
  gboolean pref_ask;
  gboolean pref_start_app;
  gboolean pref_ignore;
  gboolean pref_open_folder;
  g_autoptr(GAppInfo) info = NULL;
  g_autofree gchar *content_type = NULL;

  content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (button));

  /* fetch preferences for this content type */
  autorun_get_preferences (self, content_type,
                           &pref_start_app, &pref_ignore, &pref_open_folder);
  pref_ask = !pref_start_app && !pref_ignore && !pref_open_folder;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (button));

  /* append the separator only if we have >= 1 apps in the chooser */
  if (info != NULL) {
    gtk_app_chooser_button_append_separator (button);
  }

  gtk_app_chooser_button_append_custom_item (button, CUSTOM_ITEM_ASK,
                                             _("Ask what to do"),
                                             NULL);

  gtk_app_chooser_button_append_custom_item (button, CUSTOM_ITEM_DO_NOTHING,
                                             _("Do nothing"),
                                             NULL);

  gtk_app_chooser_button_append_custom_item (button, CUSTOM_ITEM_OPEN_FOLDER,
                                             _("Open folder"),
                                             NULL);

  gtk_app_chooser_button_set_show_dialog_item (button, TRUE);

  if (heading)
    gtk_app_chooser_button_set_heading (button, _(heading));

  if (pref_ask) {
    gtk_app_chooser_button_set_active_custom_item (button, CUSTOM_ITEM_ASK);
  } else if (pref_ignore) {
    gtk_app_chooser_button_set_active_custom_item (button, CUSTOM_ITEM_DO_NOTHING);
  } else if (pref_open_folder) {
    gtk_app_chooser_button_set_active_custom_item (button, CUSTOM_ITEM_OPEN_FOLDER);
  }

  g_signal_connect_object (button, "changed",
                           G_CALLBACK (on_chooser_changed_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (button, "custom-item-activated",
                           G_CALLBACK (on_custom_item_activated_cb), self, G_CONNECT_SWAPPED);

  ellipsize_app_chooser (button);
}

static void
on_other_type_combo_box_changed (CcRemovableMediaSettings *self)
{
  GtkTreeIter iter;
  g_autofree gchar *x_content_type = NULL;

  if (!gtk_combo_box_get_active_iter (self->other_type_combo_box, &iter)) {
    return;
  }

  gtk_tree_model_get (GTK_TREE_MODEL (self->other_type_list_store), &iter,
                      1, &x_content_type,
                      -1);

  if (self->other_application_chooser != NULL) {
    gtk_box_remove (self->other_action_box, GTK_WIDGET (self->other_application_chooser));
    self->other_application_chooser = NULL;
  }

  self->other_application_chooser = GTK_APP_CHOOSER_BUTTON (gtk_app_chooser_button_new (x_content_type));
  gtk_box_append (self->other_action_box, GTK_WIDGET (self->other_application_chooser));
  prepare_chooser (self, self->other_application_chooser, NULL);

  adw_action_row_set_activatable_widget (self->other_action_row, GTK_WIDGET (self->other_application_chooser));
}

static gboolean
on_extra_options_dialog_close_request (CcRemovableMediaSettings *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self->other_type_dialog), FALSE);

  if (self->other_application_chooser != NULL) {
    gtk_box_remove (self->other_action_box, GTK_WIDGET (self->other_application_chooser));
    self->other_application_chooser = NULL;
  }

  return GDK_EVENT_PROPAGATE;
}

static void
on_extra_options_button_clicked (CcRemovableMediaSettings *self)
{
  GtkWidget *app_panel = gtk_widget_get_ancestor(GTK_WIDGET (self), CC_TYPE_APPLICATIONS_PANEL);
  CcShell *shell = cc_panel_get_shell (CC_PANEL (app_panel));
  GtkWidget *toplevel = cc_shell_get_toplevel (shell);

  gtk_window_set_transient_for (GTK_WINDOW (self->other_type_dialog), GTK_WINDOW (toplevel));
  /* update other_application_chooser */
  on_other_type_combo_box_changed (self);
  gtk_window_present (GTK_WINDOW (self->other_type_dialog));
}

#define OFFSET(x)             (G_STRUCT_OFFSET (CcRemovableMediaSettings, x))
#define WIDGET_FROM_OFFSET(x) (G_STRUCT_MEMBER (GtkWidget*, self, x))

static void
info_panel_setup_media (CcRemovableMediaSettings *self)
{
  guint n;
  GList *l, *content_types;
  GtkTreeIter iter;

  struct {
    gint widget_offset;
    const gchar *content_type;
    const gchar *heading;
  } const defs[] = {
    { OFFSET (audio_cdda_chooser), "x-content/audio-cdda", N_("Select an app for audio CDs") },
    { OFFSET (video_dvd_chooser), "x-content/video-dvd", N_("Select an app for video DVDs") },
    { OFFSET (music_player_chooser), "x-content/audio-player", N_("Select an app to run when a music player is connected") },
    { OFFSET (dcf_chooser), "x-content/image-dcf", N_("Select an app to run when a camera is connected") },
    { OFFSET (software_chooser), "x-content/unix-software", N_("Select an app for software CDs") },
  };

  struct {
    const gchar *content_type;
    const gchar *description;
  } const other_defs[] = {
    /* translators: these strings are duplicates of shared-mime-info
     * strings, just here to fix capitalization of the English originals.
     * If the shared-mime-info translation works for your language,
     * simply leave these untranslated.
     */
    { "x-content/audio-dvd", N_("audio DVD") },
    { "x-content/blank-bd", N_("blank Blu-ray disc") },
    { "x-content/blank-cd", N_("blank CD disc") },
    { "x-content/blank-dvd", N_("blank DVD disc") },
    { "x-content/blank-hddvd", N_("blank HD DVD disc") },
    { "x-content/video-bluray", N_("Blu-ray video disc") },
    { "x-content/ebook-reader", N_("e-book reader") },
    { "x-content/video-hddvd", N_("HD DVD video disc") },
    { "x-content/image-picturecd", N_("Picture CD") },
    { "x-content/video-svcd", N_("Super Video CD") },
    { "x-content/video-vcd", N_("Video CD") },
    { "x-content/win32-software", N_("Windows software") },
  };

  for (n = 0; n < G_N_ELEMENTS (defs); n++) {
    prepare_chooser (self,
                     GTK_APP_CHOOSER_BUTTON (WIDGET_FROM_OFFSET (defs[n].widget_offset)),
                     defs[n].heading);
  }

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->other_type_list_store),
                                        1, GTK_SORT_ASCENDING);


  content_types = g_content_types_get_registered ();

  for (l = content_types; l != NULL; l = l->next) {
    char *content_type = l->data;
    g_autofree char *description = NULL;

    if (!g_str_has_prefix (content_type, "x-content/"))
      continue;

    for (n = 0; n < G_N_ELEMENTS (defs); n++) {
      if (g_content_type_is_a (content_type, defs[n].content_type)) {
        goto skip;
      }
    }

    for (n = 0; n < G_N_ELEMENTS (other_defs); n++) {
       if (strcmp (content_type, other_defs[n].content_type) == 0) {
         const gchar *s = other_defs[n].description;
         if (s == _(s))
           description = g_content_type_get_description (content_type);
         else
           description = g_strdup (_(s));

         break;
       }
    }

    if (description == NULL) {
      g_debug ("Content type '%s' is missing from the info panel", content_type);
      description = g_content_type_get_description (content_type);
    }

    gtk_list_store_append (self->other_type_list_store, &iter);

    gtk_list_store_set (self->other_type_list_store, &iter,
                        0, description,
                        1, content_type,
                        -1);
  skip:
    ;
  }

  g_list_free_full (content_types, g_free);

  gtk_combo_box_set_active (self->other_type_combo_box, 0);

  g_settings_bind (self->settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   self->cd_audio_row,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (self->settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   self->dvd_video_row,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (self->settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   self->music_player_row,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (self->settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   self->photos_row,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (self->settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   self->software_row,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (self->settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   self->other_media_row,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
}


static void
cc_removable_media_settings_finalize (GObject *object)
{
  CcRemovableMediaSettings *self = CC_REMOVABLE_MEDIA_SETTINGS (object);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (cc_removable_media_settings_parent_class)->finalize (object);
}

static void
cc_removable_media_settings_dispose (GObject *object)
{
  CcRemovableMediaSettings *self = CC_REMOVABLE_MEDIA_SETTINGS (object);

  g_clear_pointer ((GtkWindow **) &self->other_type_dialog, gtk_window_destroy);

  G_OBJECT_CLASS (cc_removable_media_settings_parent_class)->dispose (object);
}

static void
cc_removable_media_settings_class_init (CcRemovableMediaSettingsClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_removable_media_settings_finalize;
  object_class->dispose = cc_removable_media_settings_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-removable-media-settings.ui");

  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, audio_cdda_chooser);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, dcf_chooser);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, music_player_chooser);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, other_type_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, other_action_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, other_action_box);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, other_type_combo_box);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, other_type_list_store);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, software_chooser);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, video_dvd_chooser);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, cd_audio_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, dvd_video_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, music_player_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, photos_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, software_row);
  gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaSettings, other_media_row);

  gtk_widget_class_bind_template_callback (widget_class, on_extra_options_dialog_close_request);
  gtk_widget_class_bind_template_callback (widget_class, on_extra_options_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_other_type_combo_box_changed);
}

static void
cc_removable_media_settings_init (CcRemovableMediaSettings *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->settings = g_settings_new (MEDIA_HANDLING_SCHEMA);

  info_panel_setup_media (self);
}

CcRemovableMediaSettings *
cc_removable_media_settings_new (void)
{
  return g_object_new (CC_TYPE_REMOVABLE_MEDIA_SETTINGS,
                       NULL);
}
