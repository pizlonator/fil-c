/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>
#include <colord.h>
#include <gtk/gtk.h>
#include <gdk/x11/gdkx.h>

#include "cc-color-calibrate.h"
#include "cc-color-cell-renderer-text.h"
#include "cc-color-panel.h"
#include "cc-color-resources.h"
#include "cc-color-common.h"
#include "cc-color-device.h"
#include "cc-color-profile.h"

struct _CcColorPanel
{
  CcPanel        parent_instance;

  CdClient      *client;
  CdDevice      *current_device;
  GPtrArray     *devices;
  GPtrArray     *sensors;
  GDBusProxy    *proxy;
  GSettings     *settings;
  GSettings     *settings_colord;
  GtkWidget     *assistant_calib;
  GtkWidget     *box_calib_brightness;
  GtkWidget     *box_calib_kind;
  GtkWidget     *box_calib_quality;
  GtkWidget     *box_calib_sensor;
  GtkWidget     *box_calib_summary;
  GtkWidget     *box_calib_temp;
  GtkWidget     *box_calib_title;
  GtkWidget     *box_devices;
  GtkWidget     *button_assign_import;
  GtkWidget     *button_assign_ok;
  GtkWidget     *button_calib_export;
  GtkWidget     *dialog_assign;
  GtkWidget     *entry_calib_title;
  GtkWidget     *label_assign_warning;
  GtkWidget     *label_calib_summary_message;
  GtkWidget     *label_no_devices;
  GtkTreeModel  *liststore_assign;
  GtkTreeModel  *liststore_calib_kind;
  GtkTreeModel  *liststore_calib_sensor;
  AdwPreferencesGroup *pref_group_devices;
  GtkWidget     *toolbar_devices;
  GtkWidget     *toolbutton_device_calibrate;
  GtkWidget     *toolbutton_device_default;
  GtkWidget     *toolbutton_device_enable;
  GtkWidget     *toolbutton_profile_add;
  GtkWidget     *toolbutton_profile_remove;
  GtkWidget     *toolbutton_profile_view;
  GtkWidget     *treeview_assign;
  GtkWidget     *treeview_calib_kind;
  GtkWidget     *treeview_calib_quality;
  GtkWidget     *treeview_calib_sensor;
  GtkWidget     *treeview_calib_temp;
  CcColorCalibrate *calibrate;
  GtkListBox    *list_box;
  gchar         *list_box_filter;
  GtkSizeGroup  *list_box_size;
  gboolean       is_live_cd;
  gboolean       model_is_changing;
};

CC_PANEL_REGISTER (CcColorPanel, cc_color_panel)

enum {
  GCM_PREFS_COMBO_COLUMN_TEXT,
  GCM_PREFS_COMBO_COLUMN_PROFILE,
  GCM_PREFS_COMBO_COLUMN_TYPE,
  GCM_PREFS_COMBO_COLUMN_WARNING_FILENAME,
  GCM_PREFS_COMBO_COLUMN_NUM_COLUMNS
};

/* for the GtkListStores */
enum {
  COLUMN_CALIB_KIND_DESCRIPTION,
  COLUMN_CALIB_KIND_CAP_VALUE,
  COLUMN_CALIB_KIND_VISIBLE,
  COLUMN_CALIB_KIND_LAST
};
enum {
  COLUMN_CALIB_QUALITY_DESCRIPTION,
  COLUMN_CALIB_QUALITY_APPROX_TIME,
  COLUMN_CALIB_QUALITY_VALUE,
  COLUMN_CALIB_QUALITY_LAST
};
enum {
  COLUMN_CALIB_SENSOR_OBJECT,
  COLUMN_CALIB_SENSOR_DESCRIPTION,
  COLUMN_CALIB_SENSOR_LAST
};
enum {
  COLUMN_CALIB_TEMP_DESCRIPTION,
  COLUMN_CALIB_TEMP_VALUE_K,
  COLUMN_CALIB_TEMP_LAST
};

#define COLORD_SETTINGS_SCHEMA                          "org.freedesktop.ColorHelper"
#define GCM_SETTINGS_SCHEMA                             "org.gnome.settings-daemon.plugins.color"
#define GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD      "recalibrate-printer-threshold"
#define GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD      "recalibrate-display-threshold"

/* max number of devices and profiles to cause auto-expand at startup */
#define GCM_PREFS_MAX_DEVICES_PROFILES_EXPANDED         5

static void gcm_prefs_profile_add_cb (CcColorPanel *self);
static void gcm_prefs_refresh_toolbar_buttons (CcColorPanel *self);

static void
gcm_prefs_combobox_add_profile (CcColorPanel *self,
                                CdProfile *profile,
                                GtkTreeIter *iter)
{
  const gchar *id;
  GtkTreeIter iter_tmp;
  g_autoptr(GString) string = NULL;
  gchar *escaped = NULL;
  guint kind = 0;
  const gchar *warning = NULL;
#if CD_CHECK_VERSION(0,1,25)
  gchar **warnings;
#endif

  /* iter is optional */
  if (iter == NULL)
    iter = &iter_tmp;

  /* use description */
  string = g_string_new (cd_profile_get_title (profile));

  /* any source prefix? */
  id = cd_profile_get_metadata_item (profile,
                                     CD_PROFILE_METADATA_DATA_SOURCE);
  if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0)
    {
      /* TRANSLATORS: this is a profile prefix to signify the
       * profile has been auto-generated for this hardware */
      g_string_prepend (string, _("Default: "));
      kind = 1;
    }
#if CD_CHECK_VERSION(0,1,14)
  if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0)
    {
      /* TRANSLATORS: this is a profile prefix to signify the
       * profile his a standard space like AdobeRGB */
      g_string_prepend (string, _("Colorspace: "));
      kind = 2;
    }
  if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_TEST) == 0)
    {
      /* TRANSLATORS: this is a profile prefix to signify the
       * profile is a test profile */
      g_string_prepend (string, _("Test profile: "));
      kind = 3;
    }
#endif

  /* is the profile faulty */
#if CD_CHECK_VERSION(0,1,25)
  warnings = cd_profile_get_warnings (profile);
  if (warnings != NULL && warnings[0] != NULL)
    warning = "dialog-warning-symbolic";
#endif

  escaped = g_markup_escape_text (string->str, -1);
  gtk_list_store_append (GTK_LIST_STORE (self->liststore_assign), iter);
  gtk_list_store_set (GTK_LIST_STORE (self->liststore_assign), iter,
                      GCM_PREFS_COMBO_COLUMN_TEXT, escaped,
                      GCM_PREFS_COMBO_COLUMN_PROFILE, profile,
                      GCM_PREFS_COMBO_COLUMN_TYPE, kind,
                      GCM_PREFS_COMBO_COLUMN_WARNING_FILENAME, warning,
                      -1);
}

static gboolean
gcm_prefs_ensure_connected_profile (CcColorPanel *self,
                                    CdProfile *profile)
{
  gboolean ret;
  g_autoptr(GError) error = NULL;

  if (cd_profile_get_connected (profile))
    return TRUE;

  ret = cd_profile_connect_sync (profile,
                                 cc_panel_get_cancellable (CC_PANEL (self)),
                                 &error);
  if (!ret)
    g_warning ("failed to get profile: %s", error->message);

  return ret;
}

static void
gcm_prefs_default_cb (CcColorPanel *self)
{
  g_autoptr(CdProfile) profile = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;

  /* TODO: check if the profile is already systemwide */
  profile = cd_device_get_default_profile (self->current_device);
  if (profile == NULL)
    return;

  if (!gcm_prefs_ensure_connected_profile (self, profile))
    return;

  /* install somewhere out of $HOME */
  ret = cd_profile_install_system_wide_sync (profile,
                                             cc_panel_get_cancellable (CC_PANEL (self)),
                                             &error);
  if (!ret)
    g_warning ("failed to set profile system-wide: %s",
               error->message);
}

static void
icc_prefs_imported_cb (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  CcColorPanel *self = CC_COLOR_PANEL (user_data);
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(CdProfile) profile = NULL;

  file = gtk_file_dialog_open_finish (dialog, res, &error);
  if (file == NULL)
    {
      g_warning ("Failed to get ICC file: %s", error->message);
      gtk_widget_set_visible (GTK_WIDGET (self->dialog_assign), FALSE);
      return;
    }

#if CD_CHECK_VERSION(0,1,12)
  profile = cd_client_import_profile_sync (self->client,
                                           file,
                                           cc_panel_get_cancellable (CC_PANEL (self)),
                                           &error);
  if (profile == NULL)
    {
      g_warning ("failed to get imported profile: %s", error->message);
      return;
    }
#endif

  /* add to list view */
  gcm_prefs_profile_add_cb (self);
}

static void
gcm_prefs_file_chooser_get_icc_profile (CcColorPanel *self)
{
  g_autoptr(GFile) current_folder = NULL;
  GtkWindow *window;
  GtkFileDialog *dialog;
  GtkFileFilter *filter;
  GListStore *filters;

  /* create new dialog */
  window = GTK_WINDOW (self->dialog_assign);
  dialog = gtk_file_dialog_new ();
  /* TRANSLATORS: an ICC profile is a file containing colorspace data */
  gtk_file_dialog_set_title (dialog, _("Select ICC Profile File"));
  gtk_file_dialog_set_modal (dialog, TRUE);

  gtk_file_dialog_set_accept_label (dialog, _("Import"));
  current_folder = g_file_new_for_path (g_get_home_dir ());
  gtk_file_dialog_set_initial_folder (dialog, current_folder);

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  /* setup the filter */
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "application/vnd.iccprofile");

  /* TRANSLATORS: filter name on the file->open dialog */
  gtk_file_filter_set_name (filter, _("Supported ICC profiles"));
  g_list_store_append (filters, filter);

  /* setup the all files filter */
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*");
  /* TRANSLATORS: filter name on the file->open dialog */
  gtk_file_filter_set_name (filter, _("All files"));
  g_list_store_append (filters, filter);

  gtk_file_dialog_open (dialog, window, NULL,
                        icc_prefs_imported_cb, self);
}

static void
gcm_prefs_calib_cancel_cb (CcColorPanel *self)
{
  gtk_widget_set_visible (self->assistant_calib, FALSE);
}

static gboolean
gcm_prefs_calib_delayed_complete_cb (gpointer user_data)
{
  CcColorPanel *self = CC_COLOR_PANEL (user_data);
  GtkAssistant *assistant;

  assistant = GTK_ASSISTANT (self->assistant_calib);
  gtk_assistant_set_page_complete (assistant, self->box_calib_brightness, TRUE);
  return FALSE;
}

static void
gcm_prefs_calib_apply_cb (CcColorPanel *self)
{
  gboolean ret;
  g_autoptr(GError) error = NULL;
  GtkWindow *window = NULL;

  /* setup the calibration object with items that can fail */
  ret = cc_color_calibrate_setup (self->calibrate,
                                  &error);
  if (!ret)
    {
      g_warning ("failed to setup calibrate: %s", error->message);
      return;
    }

  /* actually start the calibration */
  window = GTK_WINDOW (self->assistant_calib);
  ret = cc_color_calibrate_start (self->calibrate,
                                  window,
                                  &error);
  if (!ret)
    {
      g_warning ("failed to start calibrate: %s", error->message);
      gtk_window_close (window);
      return;
    }

  /* if we are a LiveCD then don't close the window as there is another
   * summary pane with the export button */
  if (!self->is_live_cd)
    gtk_window_close (window);
}

static void
gcm_prefs_calib_temp_treeview_clicked_cb (CcColorPanel *self,
                                          GtkTreeSelection *selection)
{
  gboolean ret;
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint target_whitepoint;
  GtkAssistant *assistant;

  /* check to see if anything is selected */
  ret = gtk_tree_selection_get_selected (selection, &model, &iter);
  assistant = GTK_ASSISTANT (self->assistant_calib);
  gtk_assistant_set_page_complete (assistant, self->box_calib_temp, ret);
  if (!ret)
    return;

  gtk_tree_model_get (model, &iter,
                      COLUMN_CALIB_TEMP_VALUE_K, &target_whitepoint,
                      -1);
  cc_color_calibrate_set_temperature (self->calibrate, target_whitepoint);
}

static void
gcm_prefs_calib_kind_treeview_clicked_cb (CcColorPanel *self,
                                          GtkTreeSelection *selection)
{
  CdSensorCap device_kind;
  gboolean ret;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkAssistant *assistant;

  /* check to see if anything is selected */
  ret = gtk_tree_selection_get_selected (selection, &model, &iter);
  assistant = GTK_ASSISTANT (self->assistant_calib);
  gtk_assistant_set_page_complete (assistant, self->box_calib_kind, ret);
  if (!ret)
    return;

  /* save the values if we have a selection */
  gtk_tree_model_get (model, &iter,
                      COLUMN_CALIB_KIND_CAP_VALUE, &device_kind,
                      -1);
  cc_color_calibrate_set_kind (self->calibrate, device_kind);
}

static void
gcm_prefs_calib_quality_treeview_clicked_cb (CcColorPanel *self,
                                             GtkTreeSelection *selection)
{
  CdProfileQuality quality;
  gboolean ret;
  GtkAssistant *assistant;
  GtkTreeIter iter;
  GtkTreeModel *model;

  /* check to see if anything is selected */
  ret = gtk_tree_selection_get_selected (selection, &model, &iter);
  assistant = GTK_ASSISTANT (self->assistant_calib);
  gtk_assistant_set_page_complete (assistant, self->box_calib_quality, ret);
  if (!ret)
    return;

  /* save the values if we have a selection */
  gtk_tree_model_get (model, &iter,
                      COLUMN_CALIB_QUALITY_VALUE, &quality,
                      -1);
  cc_color_calibrate_set_quality (self->calibrate, quality);
}

static gboolean
gcm_prefs_calib_set_sensor_cap_supported_cb (GtkTreeModel *model,
                                             GtkTreePath *path,
                                             GtkTreeIter *iter,
                                             gpointer data)
{
  CdSensorCap cap;
  CdSensor *sensor = CD_SENSOR (data);
  gboolean supported;

  gtk_tree_model_get (model, iter,
                      COLUMN_CALIB_KIND_CAP_VALUE, &cap,
                      -1);
  supported = cd_sensor_has_cap (sensor, cap);
  g_debug ("%s(%s) is %s",
           cd_sensor_get_model (sensor),
           cd_sensor_cap_to_string (cap),
           supported ? "supported" : "not-supported");
  gtk_list_store_set (GTK_LIST_STORE (model), iter,
                      COLUMN_CALIB_KIND_VISIBLE, supported,
                      -1);
  return FALSE;
}

static guint8
_cd_bitfield_popcount (guint64 bitfield)
{
  guint8 i;
  guint8 tmp = 0;
  for (i = 0; i < 64; i++)
    tmp += cd_bitfield_contain (bitfield, i);
  return tmp;
}

static void
gcm_prefs_calib_set_sensor (CcColorPanel *self,
                            CdSensor *sensor)
{
  guint64 caps;
  guint8 i;

  /* use this sensor for calibration */
  cc_color_calibrate_set_sensor (self->calibrate, sensor);

  /* hide display types the sensor does not support */
  gtk_tree_model_foreach (self->liststore_calib_kind,
                          gcm_prefs_calib_set_sensor_cap_supported_cb,
                          sensor);

  /* if the sensor only supports one kind then do not show the panel at all */
  caps = cd_sensor_get_caps (sensor);
  if (_cd_bitfield_popcount (caps) == 1)
    {
      gtk_widget_set_visible (self->box_calib_kind, FALSE);
      for (i = 0; i < CD_SENSOR_CAP_LAST; i++)
        {
          if (cd_bitfield_contain (caps, i))
            cc_color_calibrate_set_kind (self->calibrate, i);
        }
    }
  else
    {
      cc_color_calibrate_set_kind (self->calibrate, CD_SENSOR_CAP_UNKNOWN);
      gtk_widget_set_visible (self->box_calib_kind, TRUE);
    }
}

static void
gcm_prefs_calib_sensor_treeview_clicked_cb (CcColorPanel *self,
                                            GtkTreeSelection *selection)
{
  gboolean ret;
  GtkTreeIter iter;
  GtkTreeModel *model;
  g_autoptr(CdSensor) sensor = NULL;
  GtkAssistant *assistant;

  /* check to see if anything is selected */
  ret = gtk_tree_selection_get_selected (selection, &model, &iter);
  assistant = GTK_ASSISTANT (self->assistant_calib);
  gtk_assistant_set_page_complete (assistant, self->box_calib_sensor, ret);
  if (!ret)
    return;

  /* save the values if we have a selection */
  gtk_tree_model_get (model, &iter,
                      COLUMN_CALIB_SENSOR_OBJECT, &sensor,
                      -1);
  gcm_prefs_calib_set_sensor (self, sensor);
}

static void
gcm_prefs_calibrate_display (CcColorPanel *self)
{
  GtkAssistant *assistant;
  CdSensor *sensor_tmp;
  const gchar *tmp;
  GtkTreeIter iter;
  guint i;

  /* set target device */
  cc_color_calibrate_set_device (self->calibrate, self->current_device);

  assistant = GTK_ASSISTANT (self->assistant_calib);
  gtk_assistant_set_page_complete (assistant, self->box_calib_brightness, FALSE);
  gtk_assistant_set_page_complete (assistant, self->box_calib_temp, FALSE);
  gtk_assistant_set_page_complete (assistant, self->box_calib_kind, FALSE);
  gtk_assistant_set_page_complete (assistant, self->box_calib_sensor, FALSE);

  /* add sensors to list */
  gtk_list_store_clear (GTK_LIST_STORE (self->liststore_calib_sensor));
  if (self->sensors->len > 1)
    {
      for (i = 0; i < self->sensors->len; i++)
        {
          sensor_tmp = g_ptr_array_index (self->sensors, i);
          gtk_list_store_append (GTK_LIST_STORE (self->liststore_calib_sensor), &iter);
          gtk_list_store_set (GTK_LIST_STORE (self->liststore_calib_sensor), &iter,
                              COLUMN_CALIB_SENSOR_OBJECT, sensor_tmp,
                              COLUMN_CALIB_SENSOR_DESCRIPTION, cd_sensor_get_model (sensor_tmp),
                              -1);
        }
      gtk_widget_set_visible (self->box_calib_sensor, TRUE);
    }
  else
    {
      sensor_tmp = g_ptr_array_index (self->sensors, 0);
      gcm_prefs_calib_set_sensor (self, sensor_tmp);
      gtk_widget_set_visible (self->box_calib_sensor, FALSE);
    }

  /* set default profile title */
  tmp = cd_device_get_model (self->current_device);
  if (tmp == NULL)
    tmp = cd_device_get_vendor (self->current_device);
  if (tmp == NULL)
    tmp = _("Screen");
  gtk_editable_set_text (GTK_EDITABLE (self->entry_calib_title), tmp);
  cc_color_calibrate_set_title (self->calibrate, tmp);

  /* set the display whitepoint to D65 by default */
  //FIXME?

  /* show ui */
  gtk_window_set_transient_for (GTK_WINDOW (self->assistant_calib),
                                GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
  gtk_widget_set_visible (self->assistant_calib, TRUE);
}

static void
gcm_prefs_title_entry_changed_cb (CcColorPanel *self)
{
  GtkAssistant *assistant;
  const gchar *value;

  assistant = GTK_ASSISTANT (self->assistant_calib);
  value = gtk_editable_get_text (GTK_EDITABLE (self->entry_calib_title));
  cc_color_calibrate_set_title (self->calibrate, value);
  gtk_assistant_set_page_complete (assistant, self->box_calib_title, value[0] != '\0');
}

static void
gcm_prefs_calibrate_cb (CcColorPanel *self)
{
  GtkNative *native;
  GdkSurface *surface;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  guint xid = 0;
  g_autoptr(GPtrArray) argv = NULL;

  /* use the new-style calibration helper */
  if (cd_device_get_kind (self->current_device) == CD_DEVICE_KIND_DISPLAY)
    {
      gcm_prefs_calibrate_display (self);
      return;
    }

  /* get xid */
  native = gtk_widget_get_native (GTK_WIDGET (self));
  surface = gtk_native_get_surface (native);

  if (GDK_IS_X11_SURFACE (surface))
    xid = gdk_x11_surface_get_xid (GDK_X11_SURFACE (surface));

  /* run with modal set */
  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_strdup ("gcm-calibrate"));
  g_ptr_array_add (argv, g_strdup ("--device"));
  g_ptr_array_add (argv, g_strdup (cd_device_get_id (self->current_device)));
  g_ptr_array_add (argv, g_strdup ("--parent-window"));
  g_ptr_array_add (argv, g_strdup_printf ("%i", xid));
  g_ptr_array_add (argv, NULL);
  ret = g_spawn_async (NULL, (gchar**) argv->pdata, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, NULL, &error);
  if (!ret)
    g_warning ("failed to run calibrate: %s", error->message);
}

static gboolean
gcm_prefs_is_profile_suitable_for_device (CdProfile *profile,
                                          CdDevice *device)
{
  const gchar *data_source;
  CdProfileKind profile_kind_tmp;
  CdProfileKind profile_kind;
  CdColorspace profile_colorspace;
  CdColorspace device_colorspace = 0;
  gboolean ret = FALSE;
  CdDeviceKind device_kind;
  CdStandardSpace standard_space;

  /* not the right colorspace */
  device_colorspace = cd_device_get_colorspace (device);
  profile_colorspace = cd_profile_get_colorspace (profile);
  if (device_colorspace != profile_colorspace)
    goto out;

  /* if this is a display matching with one of the standard spaces that displays
   * could emulate, also mark it as suitable */
  if (cd_device_get_kind (device) == CD_DEVICE_KIND_DISPLAY &&
      cd_profile_get_kind (profile) == CD_PROFILE_KIND_DISPLAY_DEVICE)
      {
        data_source = cd_profile_get_metadata_item (profile,
                                                    CD_PROFILE_METADATA_STANDARD_SPACE);
        standard_space = cd_standard_space_from_string (data_source);
        if (standard_space == CD_STANDARD_SPACE_SRGB ||
            standard_space == CD_STANDARD_SPACE_ADOBE_RGB)
          {
            ret = TRUE;
            goto out;
          }
      }

  /* not the correct kind */
  device_kind = cd_device_get_kind (device);
  profile_kind_tmp = cd_profile_get_kind (profile);
  profile_kind = cd_device_kind_to_profile_kind (device_kind);
  if (profile_kind_tmp != profile_kind)
    goto out;

  /* ignore the colorspace profiles */
  data_source = cd_profile_get_metadata_item (profile,
                                              CD_PROFILE_METADATA_DATA_SOURCE);
  if (g_strcmp0 (data_source, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0)
    goto out;

  /* success */
  ret = TRUE;
out:
  return ret;
}

static gint
gcm_prefs_combo_sort_func_cb (GtkTreeModel *model,
                              GtkTreeIter *a,
                              GtkTreeIter *b,
                              gpointer user_data)
{
  gint type_a, type_b;
  g_autofree gchar *text_a = NULL;
  g_autofree gchar *text_b = NULL;

  /* get data from model */
  gtk_tree_model_get (model, a,
                      GCM_PREFS_COMBO_COLUMN_TYPE, &type_a,
                      GCM_PREFS_COMBO_COLUMN_TEXT, &text_a,
                      -1);
  gtk_tree_model_get (model, b,
                      GCM_PREFS_COMBO_COLUMN_TYPE, &type_b,
                      GCM_PREFS_COMBO_COLUMN_TEXT, &text_b,
                      -1);

  /* prefer normal type profiles over the 'Other Profile...' entry */
  if (type_a < type_b)
    return -1;
  else if (type_a > type_b)
    return 1;
  else
    return g_strcmp0 (text_a, text_b);
}

static gboolean
gcm_prefs_ensure_connected_profiles (CcColorPanel *self,
                                     GPtrArray *profiles)
{
  guint i;

  for (i = 0; i < profiles->len; i++)
    {
      CdProfile *profile = g_ptr_array_index (profiles, i);

      if (!gcm_prefs_ensure_connected_profile (self, profile))
        return FALSE;
    }

  return TRUE;
}

static gboolean
gcm_prefs_profile_exists_in_array (GPtrArray *array, CdProfile *profile)
{
  CdProfile *profile_tmp;
  guint i;

  for (i = 0; i < array->len; i++)
    {
      profile_tmp = g_ptr_array_index (array, i);
      if (cd_profile_equal (profile, profile_tmp))
         return TRUE;
    }
  return FALSE;
}

static void
gcm_prefs_add_profiles_suitable_for_devices (CcColorPanel *self,
                                             GPtrArray *profiles)
{
  CdProfile *profile_tmp;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) profile_array = NULL;
  GtkTreeIter iter;
  guint i;

  gtk_list_store_clear (GTK_LIST_STORE (self->liststore_assign));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->liststore_assign),
                                        GCM_PREFS_COMBO_COLUMN_TEXT,
                                        GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (self->liststore_assign),
                                   GCM_PREFS_COMBO_COLUMN_TEXT,
                                   gcm_prefs_combo_sort_func_cb,
                                   self->liststore_assign, NULL);

  gtk_widget_set_visible (self->label_assign_warning, FALSE);

  /* get profiles */
  profile_array = cd_client_get_profiles_sync (self->client,
                                               cc_panel_get_cancellable (CC_PANEL (self)),
                                               &error);
  if (profile_array == NULL)
    {
      g_warning ("failed to get profiles: %s",
           error->message);
      return;
    }

  if (!gcm_prefs_ensure_connected_profiles (self, profile_array))
    return;

  /* add profiles of the right kind */
  for (i = 0; i < profile_array->len; i++)
    {
      profile_tmp = g_ptr_array_index (profile_array, i);

      /* don't add any of the already added profiles */
      if (profiles != NULL)
        {
          if (gcm_prefs_profile_exists_in_array (profiles, profile_tmp))
            continue;
        }

      /* only add correct types */
      ret = gcm_prefs_is_profile_suitable_for_device (profile_tmp,
                                                      self->current_device);
      if (!ret)
        continue;

#if CD_CHECK_VERSION(0,1,13)
      /* ignore profiles from other user accounts */
      if (!cd_profile_has_access (profile_tmp))
        continue;
#endif

      /* add */
      gcm_prefs_combobox_add_profile (self,
                                      profile_tmp,
                                      &iter);
    }
}

static void
profile_exported_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  CdProfile *profile = CD_PROFILE (user_data);
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) source = NULL;
  g_autoptr(GFile) destination = NULL;
  gboolean ret;

  source = g_file_new_for_path (cd_profile_get_filename (profile));
  destination = gtk_file_dialog_save_finish (dialog, res, &error);

  if (!destination)
    {
      g_warning ("Failed to copy profile: %s", error->message);
      return;
    }

  ret = g_file_copy (source,
                     destination,
                     G_FILE_COPY_OVERWRITE,
                     NULL,
                     NULL,
                     NULL,
                     &error);
  if (!ret)
    g_warning ("Failed to copy profile: %s", error->message);
}

static void
gcm_prefs_calib_export_cb (CcColorPanel *self)
{
  CdProfile *profile;
  gboolean ret;
  g_autofree gchar *default_name = NULL;
  g_autoptr(GError) error = NULL;
  GtkFileDialog *dialog;

  profile = cc_color_calibrate_get_profile (self->calibrate);
  ret = cd_profile_connect_sync (profile, NULL, &error);
  if (!ret)
    {
      g_warning ("Failed to get imported profile: %s", error->message);
      return;
    }

  dialog = gtk_file_dialog_new ();
  /* TRANSLATORS: this is the dialog to save the ICC profile */
  gtk_file_dialog_set_title (dialog, _("Save Profile"));
  gtk_file_dialog_set_modal (dialog, TRUE);

  default_name = g_strdup_printf ("%s.icc", cd_profile_get_title (profile));
  gtk_file_dialog_set_initial_name (dialog, default_name);

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                        NULL,
                        profile_exported_cb,
                        profile);
}

static void
gcm_prefs_calib_export_link_cb (CcColorPanel *self,
                                const gchar *url)
{
  gtk_show_uri (GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))),
                "help:gnome-help/color-howtoimport",
                GDK_CURRENT_TIME);
}

static void
gcm_prefs_profile_add_cb (CcColorPanel *self)
{
  g_autoptr(GPtrArray) profiles = NULL;

  /* add profiles of the right kind */
  profiles = cd_device_get_profiles (self->current_device);
  gcm_prefs_ensure_connected_profiles (self, profiles);
  gcm_prefs_add_profiles_suitable_for_devices (self, profiles);

  /* make insensitive until we have a selection */
  gtk_widget_set_sensitive (self->button_assign_ok, FALSE);

  /* show the dialog */
  gtk_window_set_transient_for (GTK_WINDOW (self->dialog_assign),
                                GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));

  gtk_window_present (GTK_WINDOW (self->dialog_assign));
}

static void
gcm_prefs_profile_remove_cb (CcColorPanel *self)
{
  CdProfile *profile;
  gboolean ret = FALSE;
  g_autoptr(GError) error = NULL;
  GtkListBoxRow *row;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (self->list_box);
  if (row == NULL)
    return;
  profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        return;
    }

  /* just remove it, the list store will get ::changed */
  ret = cd_device_remove_profile_sync (self->current_device,
                                       profile,
                                       cc_panel_get_cancellable (CC_PANEL (self)),
                                       &error);
  if (!ret)
    g_warning ("failed to remove profile: %s", error->message);
}

static void
gcm_prefs_make_profile_default_cb (GObject *object,
                                   GAsyncResult *res,
                                   CcColorPanel *self)
{
  CdDevice *device = CD_DEVICE (object);
  gboolean ret = FALSE;
  g_autoptr(GError) error = NULL;

  ret = cd_device_make_profile_default_finish (device,
                                               res,
                                               &error);
  if (!ret)
    {
      g_warning ("failed to set default profile on %s: %s",
                 cd_device_get_id (device),
                 error->message);
    }
  else
    {
      gcm_prefs_refresh_toolbar_buttons (self);
    }
}

static void
gcm_prefs_device_profile_enable_cb (CcColorPanel *self)
{
  CdProfile *profile;
  GtkListBoxRow *row;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (self->list_box);
  if (row == NULL)
    return;
  profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        return;
    }

  /* just set it default */
  g_debug ("setting %s default on %s",
           cd_profile_get_id (profile),
           cd_device_get_id (self->current_device));
  cd_device_make_profile_default (self->current_device,
                                  profile,
                                  cc_panel_get_cancellable (CC_PANEL (self)),
                                  (GAsyncReadyCallback) gcm_prefs_make_profile_default_cb,
                                  self);
}

static void
gcm_prefs_profile_view (CcColorPanel *self, CdProfile *profile)
{
  GtkNative *native;
  GdkSurface *surface;
  g_autoptr(GPtrArray) argv = NULL;
  guint xid = 0;
  gboolean ret;
  g_autoptr(GError) error = NULL;

  /* get xid */
  native = gtk_widget_get_native (GTK_WIDGET (self));
  surface = gtk_native_get_surface (native);

  if (GDK_IS_X11_SURFACE (surface))
    xid = gdk_x11_surface_get_xid (GDK_X11_SURFACE (surface));

  /* open up gcm-viewer as a info pane */
  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_strdup ("gcm-viewer"));
  g_ptr_array_add (argv, g_strdup ("--profile"));
  g_ptr_array_add (argv, g_strdup (cd_profile_get_id (profile)));
  g_ptr_array_add (argv, g_strdup ("--parent-window"));
  g_ptr_array_add (argv, g_strdup_printf ("%i", xid));
  g_ptr_array_add (argv, NULL);
  ret = g_spawn_async (NULL, (gchar**) argv->pdata, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, NULL, &error);
  if (!ret)
    g_warning ("failed to run calibrate: %s", error->message);
}

static gboolean
gcm_prefs_profile_assign_link_activate_cb (CcColorPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  g_autoptr(CdProfile) profile = NULL;
  GtkTreeSelection *selection;

  /* get the selected profile */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_assign));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter,
                        GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
                        -1);

  if (profile == NULL)
    {
      g_warning ("failed to get the selected profile");
      return TRUE;
    }

  /* show it in the viewer */
  gcm_prefs_profile_view (self, profile);

  return TRUE;
}

static void
gcm_prefs_profile_view_cb (CcColorPanel *self)
{
  CdProfile *profile;
  GtkListBoxRow *row;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (self->list_box);
  if (row == NULL)
    return;
  profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        return;
    }

  /* open up gcm-viewer as a info pane */
  gcm_prefs_profile_view (self, profile);
}

static void
gcm_prefs_button_assign_ok_cb (CcColorPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  g_autoptr(CdProfile) profile = NULL;
  gboolean ret = FALSE;
  g_autoptr(GError) error = NULL;
  GtkTreeSelection *selection;

  /* hide window */
  gtk_window_close (GTK_WINDOW (self->dialog_assign));

  /* get the selected profile */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_assign));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;
  gtk_tree_model_get (model, &iter,
                      GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
                      -1);
  if (profile == NULL)
    {
      g_warning ("failed to get the active profile");
      return;
    }

  /* if the device is disabled, enable the device so that we can
   * add color profiles to it */
  if (!cd_device_get_enabled (self->current_device))
    {
      ret = cd_device_set_enabled_sync (self->current_device,
                                        TRUE,
                                        cc_panel_get_cancellable (CC_PANEL (self)),
                                        &error);
      if (!ret)
        {
          g_warning ("failed to enabled device: %s", error->message);
          return;
        }
    }

  /* just add it, the list store will get ::changed */
  ret = cd_device_add_profile_sync (self->current_device,
                                    CD_DEVICE_RELATION_HARD,
                                    profile,
                                    cc_panel_get_cancellable (CC_PANEL (self)),
                                    &error);
  if (!ret)
    {
      g_warning ("failed to add: %s", error->message);
      return;
    }

  /* make it default */
  cd_device_make_profile_default (self->current_device,
                                  profile,
                                  cc_panel_get_cancellable (CC_PANEL (self)),
                                  (GAsyncReadyCallback) gcm_prefs_make_profile_default_cb,
                                  self);
}

static void
gcm_prefs_add_profiles_columns (CcColorPanel *self,
                                GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  /* text */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", GCM_PREFS_COMBO_COLUMN_TEXT);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (treeview, column);

  /* image */
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "icon-name", GCM_PREFS_COMBO_COLUMN_WARNING_FILENAME);
  gtk_tree_view_append_column (treeview, column);
}

static void
gcm_prefs_set_calibrate_button_sensitivity (CcColorPanel *self)
{
  gboolean ret = FALSE;
  const gchar *tooltip;
  CdDeviceKind kind;
  CdSensor *sensor_tmp;

  /* TRANSLATORS: this is when the button is sensitive */
  tooltip = _("Create a color profile for the selected device");

  /* no device selected */
  if (self->current_device == NULL)
    goto out;

  /* are we a display */
  kind = cd_device_get_kind (self->current_device);
  if (kind == CD_DEVICE_KIND_DISPLAY)
    {

      /* find whether we have hardware installed */
      if (self->sensors == NULL || self->sensors->len == 0)
        {
          /* TRANSLATORS: this is when the button is insensitive */
          tooltip = _("The measuring instrument is not detected. Please check it is turned on and correctly connected.");
          goto out;
        }

      /* success */
      ret = TRUE;

    }
  else if (kind == CD_DEVICE_KIND_SCANNER ||
           kind == CD_DEVICE_KIND_CAMERA ||
           kind == CD_DEVICE_KIND_WEBCAM)
    {

      /* TODO: find out if we can scan using gnome-scan */
      ret = TRUE;

    }
  else if (kind == CD_DEVICE_KIND_PRINTER)
    {

    /* find whether we have hardware installed */
    if (self->sensors == NULL || self->sensors->len == 0)
      {
        /* TRANSLATORS: this is when the button is insensitive */
        tooltip = _("The measuring instrument is not detected. Please check it is turned on and correctly connected.");
        goto out;
      }

    /* find whether we have hardware installed */
    sensor_tmp = g_ptr_array_index (self->sensors, 0);
    ret = cd_sensor_has_cap (sensor_tmp, CD_SENSOR_CAP_PRINTER);
    if (!ret)
      {
        /* TRANSLATORS: this is when the button is insensitive */
        tooltip = _("The measuring instrument does not support printer profiling.");
        goto out;
      }

    /* success */
    ret = TRUE;

    }
  else
    {
      /* TRANSLATORS: this is when the button is insensitive */
      tooltip = _("The device type is not currently supported.");
    }
out:
  /* control the tooltip and sensitivity of the button */
  gtk_widget_set_tooltip_text (self->toolbutton_device_calibrate, tooltip);
  gtk_widget_set_sensitive (self->toolbutton_device_calibrate, ret);
}

static void
gcm_prefs_device_clicked (CcColorPanel *self, CdDevice *device)
{
  /* we have a new device */
  g_debug ("selected device is: %s",
           cd_device_get_id (device));

  /* can this device calibrate */
  gcm_prefs_set_calibrate_button_sensitivity (self);
}

static void
gcm_prefs_profile_clicked (CcColorPanel *self, CdProfile *profile, CdDevice *device)
{
  g_autofree gchar *s = NULL;

  /* get profile */
  g_debug ("selected profile = %s",
     cd_profile_get_filename (profile));

  /* allow getting profile info */
  if (cd_profile_get_filename (profile) != NULL &&
      (s = g_find_program_in_path ("gcm-viewer")) != NULL)
    gtk_widget_set_sensitive (self->toolbutton_profile_view, TRUE);
  else
    gtk_widget_set_sensitive (self->toolbutton_profile_view, FALSE);
}

static void
gcm_prefs_profiles_treeview_clicked_cb (CcColorPanel *self,
                                        GtkTreeSelection *selection)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  g_autoptr(CdProfile) profile = NULL;
#if CD_CHECK_VERSION(0,1,25)
  gchar **warnings;
#endif

  /* get selection */
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;
  gtk_tree_model_get (model, &iter,
                      GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
                      -1);

  /* as soon as anything is selected, make the Add button sensitive */
  gtk_widget_set_sensitive (self->button_assign_ok, TRUE);

  /* is the profile faulty */
#if CD_CHECK_VERSION(0,1,25)
  warnings = cd_profile_get_warnings (profile);
  gtk_widget_set_visible (self->label_assign_warning, warnings != NULL && warnings[0] != NULL);
#else
  gtk_widget_set_visible (self->label_assign_warning, FALSE);
#endif
}

static void
gcm_prefs_profiles_row_activated_cb (CcColorPanel *self,
                                     GtkTreePath *path)
{
  GtkTreeIter iter;
  gboolean ret;

  ret = gtk_tree_model_get_iter (gtk_tree_view_get_model (GTK_TREE_VIEW (self->treeview_assign)), &iter, path);
  if (!ret)
    return;
  gcm_prefs_button_assign_ok_cb (self);
}


static void
gcm_prefs_button_assign_import_cb (CcColorPanel *self)
{
  gcm_prefs_file_chooser_get_icc_profile (self);
}

static void
gcm_prefs_sensor_coldplug (CcColorPanel *self)
{
  CdSensor *sensor_tmp;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) sensors = NULL;
  guint i;

  /* unref old */
  g_clear_pointer (&self->sensors, g_ptr_array_unref);

  /* no present */
  sensors = cd_client_get_sensors_sync (self->client, NULL, &error);
  if (sensors == NULL)
    {
      g_warning ("%s", error->message);
      return;
    }
  if (sensors->len == 0)
    return;

  /* save a copy of the sensor list */
  self->sensors = g_ptr_array_ref (sensors);

  /* connect to each sensor */
  for (i = 0; i < sensors->len; i++)
    {
      sensor_tmp = g_ptr_array_index (sensors, i);
      ret = cd_sensor_connect_sync (sensor_tmp, NULL, &error);
      if (!ret)
        {
          g_warning ("%s", error->message);
          return;
        }
    }
}

static void
gcm_prefs_client_sensor_changed_cb (CdClient *client,
                                    CdSensor *sensor,
                                    CcColorPanel *self)
{
  gcm_prefs_sensor_coldplug (self);
  gcm_prefs_set_calibrate_button_sensitivity (self);
}

static void
gcm_prefs_add_device_profile (CcColorPanel *self,
                              CdDevice *device,
                              CdProfile *profile,
                              gboolean is_default)
{
  gboolean ret;
  g_autoptr(GError) error = NULL;
  GtkWidget *widget;

  /* get properties */
  ret = cd_profile_connect_sync (profile,
                                 cc_panel_get_cancellable (CC_PANEL (self)),
                                 &error);
  if (!ret)
    {
      g_warning ("failed to get profile: %s", error->message);
      return;
    }

  /* ignore profiles from other user accounts */
  if (!cd_profile_has_access (profile))
    {
      /* only print the filename if it exists */
      if (cd_profile_get_filename (profile) != NULL)
        {
          g_warning ("%s is not usable by this user",
                     cd_profile_get_filename (profile));
        }
      else
        {
          g_warning ("%s is not usable by this user",
                     cd_profile_get_id (profile));
        }
      return;
    }

  /* add to listbox */
  widget = cc_color_profile_new (device, profile, is_default);
  gtk_list_box_append (self->list_box, widget);
  gtk_size_group_add_widget (self->list_box_size, widget);
}

static void
gcm_prefs_add_device_profiles (CcColorPanel *self, CdDevice *device)
{
  CdProfile *profile_tmp;
  g_autoptr(GPtrArray) profiles = NULL;
  guint i;

  /* add profiles */
  profiles = cd_device_get_profiles (device);
  if (profiles == NULL)
    return;
  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      gcm_prefs_add_device_profile (self, device, profile_tmp, i == 0);
    }
}

/* find the profile in the array -- for flicker-free changes */
static gboolean
gcm_prefs_find_profile_by_object_path (GPtrArray *profiles,
                                       const gchar *object_path)
{
  CdProfile *profile_tmp;
  guint i;

  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      if (g_strcmp0 (cd_profile_get_object_path (profile_tmp), object_path) == 0)
        return TRUE;
    }
  return FALSE;
}

/* find the profile in the list view -- for flicker-free changes */
static gboolean
gcm_prefs_find_widget_by_object_path (GList *list,
                                      const gchar *object_path_device,
                                      const gchar *object_path_profile)
{
  GList *l;
  CdDevice *device_tmp;
  CdProfile *profile_tmp;

  for (l = list; l != NULL; l = l->next)
    {
      if (!CC_IS_COLOR_PROFILE (l->data))
        continue;

      /* correct device ? */
      device_tmp = cc_color_profile_get_device (CC_COLOR_PROFILE (l->data));
      if (g_strcmp0 (object_path_device,
                     cd_device_get_object_path (device_tmp)) != 0)
        {
          continue;
        }

      /* this profile */
      profile_tmp = cc_color_profile_get_profile (CC_COLOR_PROFILE (l->data));
      if (g_strcmp0 (object_path_profile,
                     cd_profile_get_object_path (profile_tmp)) == 0)
        {
          return TRUE;
        }
    }
  return FALSE;
}

static void
gcm_prefs_device_changed_cb (CcColorPanel *self, CdDevice *device)
{
  GtkWidget *child;
  CdDevice *device_tmp;
  CdProfile *profile_tmp;
  gboolean ret;
  g_autoptr(GList) list = NULL;
  GPtrArray *profiles;
  guint i;

  /* remove anything in the list view that's not in Device.Profiles */
  profiles = cd_device_get_profiles (device);
  child = gtk_widget_get_first_child (GTK_WIDGET (self->list_box));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      if (!CC_IS_COLOR_PROFILE (child))
        {
          list = g_list_prepend (list, child);
          goto next;
        }

      /* correct device ? */
      device_tmp = cc_color_profile_get_device (CC_COLOR_PROFILE (child));
      if (g_strcmp0 (cd_device_get_id (device),
                     cd_device_get_id (device_tmp)) != 0)
        {
          list = g_list_prepend (list, child);
          goto next;
        }

      /* if profile is not in Device.Profiles then remove */
      profile_tmp = cc_color_profile_get_profile (CC_COLOR_PROFILE (child));
      ret = gcm_prefs_find_profile_by_object_path (profiles,
                                                   cd_profile_get_object_path (profile_tmp));
      if (!ret)
        gtk_list_box_remove (self->list_box, child);
      else
        list = g_list_prepend (list, child);

next:
      child = next;
    }

  /* add anything in Device.Profiles that's not in the list view */
  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      ret = gcm_prefs_find_widget_by_object_path (list,
                                                  cd_device_get_object_path (device),
                                                  cd_profile_get_object_path (profile_tmp));
      if (!ret)
        gcm_prefs_add_device_profile (self, device, profile_tmp, i == 0);
    }

  /* resort */
  gtk_list_box_invalidate_sort (self->list_box);
}

static void
gcm_prefs_device_expanded_changed_cb (CcColorPanel *self,
                                      gboolean is_expanded,
                                      CcColorDevice *widget)
{
  /* ignore internal changes */
  if (self->model_is_changing)
    return;

  g_free (self->list_box_filter);
  if (is_expanded)
    {
      GtkWidget *child;

      self->list_box_filter = g_strdup (cd_device_get_id (cc_color_device_get_device (widget)));

      /* unexpand other device widgets */
      self->model_is_changing = TRUE;
      for (child = gtk_widget_get_first_child (GTK_WIDGET (self->list_box));
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!CC_IS_COLOR_DEVICE (child))
            continue;
          if (CC_COLOR_DEVICE (child) != widget)
            cc_color_device_set_expanded (CC_COLOR_DEVICE (child), FALSE);
        }
      self->model_is_changing = FALSE;

      gtk_list_box_select_row (self->list_box, GTK_LIST_BOX_ROW (widget));
    }
  else
    {
      self->list_box_filter = NULL;

      gtk_list_box_unselect_row (self->list_box, GTK_LIST_BOX_ROW (widget));
    }
  gtk_list_box_invalidate_filter (self->list_box);
}

static void
gcm_prefs_add_device (CcColorPanel *self, CdDevice *device)
{
  gboolean ret;
  g_autoptr(GError) error = NULL;
  GtkWidget *widget;

  /* get device properties */
  ret = cd_device_connect_sync (device, cc_panel_get_cancellable (CC_PANEL (self)), &error);
  if (!ret)
    {
      g_warning ("failed to connect to the device: %s", error->message);
      return;
    }

  /* add device */
  widget = cc_color_device_new (device);
  g_signal_connect_object (widget, "expanded-changed",
                           G_CALLBACK (gcm_prefs_device_expanded_changed_cb), self, G_CONNECT_SWAPPED);
  gtk_list_box_append (self->list_box, widget);
  gtk_size_group_add_widget (self->list_box_size, widget);

  /* add profiles */
  gcm_prefs_add_device_profiles (self, device);

  /* watch for changes */
  g_ptr_array_add (self->devices, g_object_ref (device));
  g_signal_connect_object (device, "changed",
                           G_CALLBACK (gcm_prefs_device_changed_cb), self, G_CONNECT_SWAPPED);
  gtk_list_box_invalidate_sort (self->list_box);
}

static void
gcm_prefs_remove_device (CcColorPanel *self, CdDevice *device)
{
  GtkWidget *child;
  CdDevice *device_tmp;

  child = gtk_widget_get_first_child (GTK_WIDGET (self->list_box));
  while (child)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      if (CC_IS_COLOR_DEVICE (child))
        device_tmp = cc_color_device_get_device (CC_COLOR_DEVICE (child));
      else
        device_tmp = cc_color_profile_get_device (CC_COLOR_PROFILE (child));
      if (g_strcmp0 (cd_device_get_object_path (device),
                     cd_device_get_object_path (device_tmp)) == 0)
        {
          gtk_list_box_remove (self->list_box, child);
        }

      child = next;
    }
  g_signal_handlers_disconnect_by_func (device,
                                        G_CALLBACK (gcm_prefs_device_changed_cb),
                                        self);
  g_ptr_array_remove (self->devices, device);
}

static void
gcm_prefs_update_device_list_extra_entry (CcColorPanel *self)
{
  GtkListBoxRow *first_row;

  /* any devices to show? */
  first_row = gtk_list_box_get_row_at_index (self->list_box, 0);
  gtk_widget_set_visible (self->label_no_devices, first_row == NULL);
  gtk_widget_set_visible (self->box_devices, first_row != NULL);

  /* if we have only one device expand it by default */
  if (first_row != NULL &&
      gtk_list_box_get_row_at_index (self->list_box, 1) == NULL)
    cc_color_device_set_expanded (CC_COLOR_DEVICE (first_row), TRUE);
}

static void
gcm_prefs_device_added_cb (CdClient *client,
                           CdDevice *device,
                           CcColorPanel *self)
{
  /* add the device */
  gcm_prefs_add_device (self, device);

  /* ensure we're not showing the 'No devices detected' entry */
  gcm_prefs_update_device_list_extra_entry (self);
}

static void
gcm_prefs_device_removed_cb (CdClient *client,
                             CdDevice *device,
                             CcColorPanel *self)
{
  /* remove from the UI */
  gcm_prefs_remove_device (self, device);

  /* ensure we showing the 'No devices detected' entry if required */
  gcm_prefs_update_device_list_extra_entry (self);
}

static void
gcm_prefs_get_devices_cb (GObject *object,
                          GAsyncResult *res,
                          gpointer user_data)
{
  CcColorPanel *self = (CcColorPanel *) user_data;
  CdClient *client = CD_CLIENT (object);
  CdDevice *device;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) devices = NULL;
  guint i;

  /* get devices and add them */
  devices = cd_client_get_devices_finish (client, res, &error);
  if (devices == NULL)
    {
      g_warning ("failed to add connected devices: %s",
                 error->message);
      return;
    }
  for (i = 0; i < devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      gcm_prefs_add_device (self, device);
    }

  /* ensure we show the 'No devices detected' entry if empty */
  gcm_prefs_update_device_list_extra_entry (self);
}

static void
gcm_prefs_list_box_row_selected_cb (CcColorPanel *self,
                                    GtkListBoxRow *row)
{
  if (!self->toolbar_devices ||
      gtk_widget_in_destruction (self->toolbar_devices))
    return;

  gcm_prefs_refresh_toolbar_buttons (self);
}

static void
gcm_prefs_refresh_toolbar_buttons (CcColorPanel *self)
{
  CdProfile *profile = NULL;
  GtkListBoxRow *row;
  gboolean is_device;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (self->list_box);

  is_device = CC_IS_COLOR_DEVICE (row);

  /* nothing selected */
  gtk_widget_set_visible (self->toolbar_devices, row != NULL);
  if (row == NULL)
    return;

  /* save current device */
  g_clear_object (&self->current_device);
  g_object_get (row, "device", &self->current_device, NULL);

  /* device actions */
  g_debug ("%s selected", is_device ? "device" : "profile");
  if (CC_IS_COLOR_DEVICE (row))
    {
      gcm_prefs_device_clicked (self, self->current_device);
      cc_color_device_set_expanded (CC_COLOR_DEVICE (row), TRUE);
    }
  else if (CC_IS_COLOR_PROFILE (row))
    {
      profile = cc_color_profile_get_profile (CC_COLOR_PROFILE (row));
      gcm_prefs_profile_clicked (self, profile, self->current_device);
    }
  else
    g_assert_not_reached ();

  gtk_widget_set_visible (self->toolbutton_device_default, !is_device && cc_color_profile_get_is_default (CC_COLOR_PROFILE (row)));
  if (profile)
    gtk_widget_set_sensitive (self->toolbutton_device_default, !cd_profile_get_is_system_wide (profile));
  gtk_widget_set_visible (self->toolbutton_device_enable, !is_device && !cc_color_profile_get_is_default (CC_COLOR_PROFILE (row)));
  gtk_widget_set_visible (self->toolbutton_device_calibrate, is_device);
  gtk_widget_set_visible (self->toolbutton_profile_add, is_device);
  gtk_widget_set_visible (self->toolbutton_profile_view, !is_device);
  gtk_widget_set_visible (self->toolbutton_profile_remove, !is_device);
}

static void
gcm_prefs_list_box_row_activated_cb (CcColorPanel *self,
                                     GtkListBoxRow *row)
{
  if (CC_IS_COLOR_PROFILE (row))
    {
      gcm_prefs_device_profile_enable_cb (self);
    }
}

static void
gcm_prefs_connect_cb (GObject *object,
                      GAsyncResult *res,
                      gpointer user_data)
{
  CcColorPanel *self;
  gboolean ret;
  g_autoptr(GError) error = NULL;

  ret = cd_client_connect_finish (CD_CLIENT (object),
                                  res,
                                  &error);
  if (!ret)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect to colord: %s", error->message);
      return;
    }

  /* Only cast the parameters after making sure it didn't fail. At this point,
   * the user can potentially already have changed to another panel, effectively
   * making user_data invalid. */
  self = CC_COLOR_PANEL (user_data);

  /* set calibrate button sensitivity */
  gcm_prefs_sensor_coldplug (self);

  /* get devices */
  cd_client_get_devices (self->client,
                         cc_panel_get_cancellable (CC_PANEL (self)),
                         gcm_prefs_get_devices_cb,
                         self);
}

static gboolean
gcm_prefs_is_livecd (void)
{
#ifdef __linux__
  gboolean ret = TRUE;
  g_autofree gchar *data = NULL;
  g_autoptr(GError) error = NULL;

  /* allow testing */
  if (g_getenv ("CC_COLOR_PANEL_IS_LIVECD") != NULL)
    return TRUE;

  /* get the kernel commandline */
  ret = g_file_get_contents ("/proc/cmdline", &data, NULL, &error);
  if (!ret)
    {
      g_warning ("failed to get kernel command line: %s",
                 error->message);
      return TRUE;
    }
  return (g_strstr_len (data, -1, "liveimg") != NULL ||
          g_strstr_len (data, -1, "casper") != NULL);
#else
  return FALSE;
#endif
}

static const char *
cc_color_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/color";
}

static void
cc_color_panel_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_color_panel_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_color_panel_dispose (GObject *object)
{
  CcColorPanel *self = CC_COLOR_PANEL (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->settings_colord);
  g_clear_object (&self->client);
  g_clear_object (&self->current_device);
  g_clear_pointer (&self->devices, g_ptr_array_unref);
  g_clear_object (&self->calibrate);
  g_clear_object (&self->list_box_size);
  g_clear_pointer (&self->sensors, g_ptr_array_unref);
  g_clear_pointer (&self->list_box_filter, g_free);
  g_clear_pointer ((GtkWindow **)&self->dialog_assign, gtk_window_destroy);

  G_OBJECT_CLASS (cc_color_panel_parent_class)->dispose (object);
}

static void
cc_color_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_color_panel_parent_class)->finalize (object);
}

static void
cc_color_panel_class_init (CcColorPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_color_panel_get_help_uri;

  object_class->get_property = cc_color_panel_get_property;
  object_class->set_property = cc_color_panel_set_property;
  object_class->dispose = cc_color_panel_dispose;
  object_class->finalize = cc_color_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/color/cc-color-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, assistant_calib);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, box_calib_brightness);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, box_calib_kind);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, box_calib_quality);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, box_calib_sensor);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, box_calib_summary);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, box_calib_temp);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, box_calib_title);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, box_devices);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, button_assign_import);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, button_assign_ok);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, button_calib_export);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, dialog_assign);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, entry_calib_title);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, label_assign_warning);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, label_calib_summary_message);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, label_no_devices);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, list_box);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, liststore_assign);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, liststore_calib_kind);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, liststore_calib_sensor);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, pref_group_devices);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, toolbar_devices);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, toolbutton_device_calibrate);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, toolbutton_device_default);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, toolbutton_device_enable);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, toolbutton_profile_add);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, toolbutton_profile_remove);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, toolbutton_profile_view);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, treeview_assign);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, treeview_calib_kind);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, treeview_calib_quality);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, treeview_calib_sensor);
  gtk_widget_class_bind_template_child (widget_class, CcColorPanel, treeview_calib_temp);
}

static gint
cc_color_panel_sort_func (GtkListBoxRow *a,
                          GtkListBoxRow *b,
                          gpointer user_data)
{
  const gchar *sort_a = NULL;
  const gchar *sort_b = NULL;
  if (CC_IS_COLOR_DEVICE (a))
    sort_a = cc_color_device_get_sortable (CC_COLOR_DEVICE (a));
  else if (CC_IS_COLOR_PROFILE (a))
    sort_a = cc_color_profile_get_sortable (CC_COLOR_PROFILE (a));
  else
    g_assert_not_reached ();
  if (CC_IS_COLOR_DEVICE (b))
    sort_b = cc_color_device_get_sortable (CC_COLOR_DEVICE (b));
  else if (CC_IS_COLOR_PROFILE (b))
    sort_b = cc_color_profile_get_sortable (CC_COLOR_PROFILE (b));
  else
    g_assert_not_reached ();
  return g_strcmp0 (sort_b, sort_a);
}

static gboolean
cc_color_panel_filter_func (GtkListBoxRow *row, void *user_data)
{
  CcColorPanel *self = CC_COLOR_PANEL (user_data);
  g_autoptr(CdDevice) device = NULL;

  /* always show all devices */
  if (CC_IS_COLOR_DEVICE (row))
    return TRUE;

  g_object_get (row, "device", &device, NULL);
  return g_strcmp0 (cd_device_get_id (device), self->list_box_filter) == 0;
}

static gboolean
cc_color_panel_treeview_quality_default_cb (GtkTreeModel *model,
                                            GtkTreePath *path,
                                            GtkTreeIter *iter,
                                            gpointer data)
{
  CdProfileQuality quality;
  GtkTreeSelection *selection = GTK_TREE_SELECTION (data);

  gtk_tree_model_get (model, iter,
                      COLUMN_CALIB_QUALITY_VALUE, &quality,
                      -1);
  if (quality == CD_PROFILE_QUALITY_MEDIUM)
    gtk_tree_selection_select_iter (selection, iter);
  return FALSE;
}

static void
gcm_prefs_calib_prepare_cb (CcColorPanel *self,
                            GtkWidget    *page)
{
  GtkTreeSelection *selection;

  /* give the user the indication they should actually manually set the
   * desired brightness rather than clicking blindly by delaying the
   * "Next" button deliberately for a second or so */
  if (page == self->box_calib_brightness)
    {
      g_timeout_add_seconds (1, gcm_prefs_calib_delayed_complete_cb, self);
      return;
    }
  else if (page == self->box_calib_temp)
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_calib_temp));
      gcm_prefs_calib_temp_treeview_clicked_cb (self, selection);
    }
  else if (page == self->box_calib_kind)
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_calib_kind));
      gcm_prefs_calib_kind_treeview_clicked_cb (self, selection);
    }
  else if (page == self->box_calib_quality)
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_calib_quality));
      gcm_prefs_calib_quality_treeview_clicked_cb (self, selection);
    }
  else if (page == self->box_calib_sensor)
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_calib_sensor));
      gcm_prefs_calib_sensor_treeview_clicked_cb (self, selection);
    }

  /* disable the brightness page as we don't want to show a 'Finished'
   * button if the user goes back at any point */
  gtk_assistant_set_page_complete (GTK_ASSISTANT (self->assistant_calib), self->box_calib_brightness, FALSE);
}

static void
cc_color_panel_init (CcColorPanel *self)
{
  GtkCellRenderer *renderer;
  GtkTreeModel *model;
  GtkTreeModel *model_filter;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  g_autofree gchar *learn_more_link = NULL;
  g_autofree gchar *panel_description = NULL;

  g_resources_register (cc_color_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  /* can do native display calibration using colord-session */
  self->calibrate = cc_color_calibrate_new ();
  cc_color_calibrate_set_quality (self->calibrate, CD_PROFILE_QUALITY_MEDIUM);

  /* setup defaults */
  self->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
  self->settings_colord = g_settings_new (COLORD_SETTINGS_SCHEMA);

  /* Translators: This will be presented as the text of a link to the documentation */
  learn_more_link = g_strdup_printf ("<a href='help:gnome-help/color-whyimportant'>%s</a>", _("Learn more"));
  /* Translators: %s is a link to the documentation with the label "Learn more" */
  panel_description = g_strdup_printf (_("Each device needs an up to date color profile to be color managed. %s"), learn_more_link);
  adw_preferences_group_set_description (self->pref_group_devices, panel_description);

  /* assign buttons */
  g_signal_connect_object (self->toolbutton_profile_add, "clicked",
                           G_CALLBACK (gcm_prefs_profile_add_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->toolbutton_profile_remove, "clicked",
                           G_CALLBACK (gcm_prefs_profile_remove_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->toolbutton_profile_view, "clicked",
                           G_CALLBACK (gcm_prefs_profile_view_cb), self, G_CONNECT_SWAPPED);

  /* href */
  g_signal_connect_object (self->label_assign_warning, "activate-link",
                           G_CALLBACK (gcm_prefs_profile_assign_link_activate_cb), self, G_CONNECT_SWAPPED);

  /* add columns to profile tree view */
  gcm_prefs_add_profiles_columns (self, GTK_TREE_VIEW (self->treeview_assign));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_assign));
  g_signal_connect_object (selection, "changed",
                           G_CALLBACK (gcm_prefs_profiles_treeview_clicked_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->treeview_assign, "row-activated",
                           G_CALLBACK (gcm_prefs_profiles_row_activated_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->toolbutton_device_default, "clicked",
                           G_CALLBACK (gcm_prefs_default_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->toolbutton_device_enable, "clicked",
                           G_CALLBACK (gcm_prefs_device_profile_enable_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->toolbutton_device_calibrate, "clicked",
                           G_CALLBACK (gcm_prefs_calibrate_cb), self, G_CONNECT_SWAPPED);

  /* set up assign dialog */
  g_signal_connect_object (self->button_assign_ok, "clicked",
                           G_CALLBACK (gcm_prefs_button_assign_ok_cb), self, G_CONNECT_SWAPPED);

  /* setup icc profiles list */
  g_signal_connect_object (self->button_assign_import, "clicked",
                           G_CALLBACK (gcm_prefs_button_assign_import_cb), self, G_CONNECT_SWAPPED);

  /* setup the calibration helper */
  g_signal_connect_object (self->assistant_calib, "apply",
                           G_CALLBACK (gcm_prefs_calib_apply_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->assistant_calib, "cancel",
                           G_CALLBACK (gcm_prefs_calib_cancel_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->assistant_calib, "close",
                           G_CALLBACK (gcm_prefs_calib_cancel_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->assistant_calib, "prepare",
                           G_CALLBACK (gcm_prefs_calib_prepare_cb),
                           self, G_CONNECT_SWAPPED);

  /* setup the calibration helper ::TreeView */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_calib_quality));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->treeview_calib_quality));
  gtk_tree_model_foreach (model,
                          cc_color_panel_treeview_quality_default_cb,
                          selection);
  g_signal_connect_object (selection, "changed",
                           G_CALLBACK (gcm_prefs_calib_quality_treeview_clicked_cb),
                           self, G_CONNECT_SWAPPED);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_QUALITY_DESCRIPTION);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->treeview_calib_quality),
                               GTK_TREE_VIEW_COLUMN (column));
  column = gtk_tree_view_column_new ();
  renderer = cc_color_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                "is-dim-label", TRUE,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_QUALITY_APPROX_TIME);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->treeview_calib_quality),
                               GTK_TREE_VIEW_COLUMN (column));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_calib_sensor));
  g_signal_connect_object (selection, "changed",
                           G_CALLBACK (gcm_prefs_calib_sensor_treeview_clicked_cb),
                           self, G_CONNECT_SWAPPED);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_SENSOR_DESCRIPTION);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->treeview_calib_sensor),
                               GTK_TREE_VIEW_COLUMN (column));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_calib_kind));
  g_signal_connect_object (selection, "changed",
                           G_CALLBACK (gcm_prefs_calib_kind_treeview_clicked_cb),
                           self, G_CONNECT_SWAPPED);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_KIND_DESCRIPTION);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->treeview_calib_kind));
  model_filter = gtk_tree_model_filter_new (model, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (self->treeview_calib_kind), model_filter);
  gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (model_filter),
                                            COLUMN_CALIB_KIND_VISIBLE);

  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->treeview_calib_kind),
                               GTK_TREE_VIEW_COLUMN (column));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->treeview_calib_temp));
  g_signal_connect_object (selection, "changed",
                           G_CALLBACK (gcm_prefs_calib_temp_treeview_clicked_cb),
                           self, G_CONNECT_SWAPPED);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "xpad", 9,
                "ypad", 9,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer,
                                      "markup", COLUMN_CALIB_TEMP_DESCRIPTION);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self->treeview_calib_temp),
                               GTK_TREE_VIEW_COLUMN (column));
  g_signal_connect_object (self->entry_calib_title, "notify::text",
                           G_CALLBACK (gcm_prefs_title_entry_changed_cb), self, G_CONNECT_SWAPPED);

  /* use a device client array */
  self->client = cd_client_new ();
  g_signal_connect_object (self->client, "device-added",
                           G_CALLBACK (gcm_prefs_device_added_cb), self, 0);
  g_signal_connect_object (self->client, "device-removed",
                           G_CALLBACK (gcm_prefs_device_removed_cb), self, 0);

  /* use a listbox for the main UI */
  gtk_list_box_set_filter_func (self->list_box,
                                cc_color_panel_filter_func,
                                self,
                                NULL);
  gtk_list_box_set_sort_func (self->list_box,
                              cc_color_panel_sort_func,
                              self,
                              NULL);
  g_signal_connect_object (self->list_box, "row-selected",
                           G_CALLBACK (gcm_prefs_list_box_row_selected_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->list_box, "row-activated",
                           G_CALLBACK (gcm_prefs_list_box_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
  self->list_box_size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  /* connect to colord */
  cd_client_connect (self->client,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     gcm_prefs_connect_cb,
                     self);

  /* use the color sensor */
  g_signal_connect_object (self->client, "sensor-added",
                           G_CALLBACK (gcm_prefs_client_sensor_changed_cb),
                           self, 0);
  g_signal_connect_object (self->client, "sensor-removed",
                           G_CALLBACK (gcm_prefs_client_sensor_changed_cb),
                           self, 0);

  /* set calibrate button sensitivity */
  gcm_prefs_set_calibrate_button_sensitivity (self);

  /* show the confirmation export page if we are running from a LiveCD */
  self->is_live_cd = gcm_prefs_is_livecd ();
  gtk_widget_set_visible (self->box_calib_summary, self->is_live_cd);
  g_signal_connect_object (self->button_calib_export, "clicked",
                           G_CALLBACK (gcm_prefs_calib_export_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->label_calib_summary_message, "activate-link",
                           G_CALLBACK (gcm_prefs_calib_export_link_cb), self, G_CONNECT_SWAPPED);
}
