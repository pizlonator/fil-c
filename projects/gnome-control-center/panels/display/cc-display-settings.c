/* cc-display-settings.c
 *
 * Copyright (C) 2007, 2008, 2018, 2019  Red Hat, Inc.
 * Copyright (C) 2013 Intel, Inc.
 *
 * Written by: Benjamin Berg <bberg@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <float.h>
#include <glib/gi18n.h>
#include <float.h>
#include <math.h>
#include "cc-display-settings.h"
#include "cc-display-config.h"

#define MAX_SCALE_BUTTONS 5

struct _CcDisplaySettings
{
  GtkBox            object;

  gboolean          updating;
  gboolean          num_scales;
  gboolean          collapsed;
  guint             idle_udpate_id;

  gboolean          has_accelerometer;
  CcDisplayConfig  *config;
  CcDisplayMonitor *selected_output;

  GListModel       *orientation_list;
  GListStore       *refresh_rate_list;
  GListStore       *resolution_list;
  GListModel       *scale_list;

  GtkWidget        *enabled_listbox;
  AdwSwitchRow     *enabled_row;
  GtkWidget        *orientation_row;
  GtkWidget        *refresh_rate_row;
  AdwExpanderRow   *refresh_rate_expander_row;
  GtkLabel         *refresh_rate_expander_suffix_label;
  AdwSwitchRow     *variable_refresh_rate_row;
  AdwComboRow      *preferred_refresh_rate_row;
  GtkWidget        *resolution_row;
  GtkWidget        *scale_bbox;
  GtkWidget        *scale_buttons_row;
  GtkWidget        *scale_combo_row;
  AdwSwitchRow     *underscanning_row;
};

typedef struct _CcDisplaySettings CcDisplaySettings;

enum {
  PROP_0,
  PROP_HAS_ACCELEROMETER,
  PROP_CONFIG,
  PROP_SELECTED_OUTPUT,
  PROP_LAST
};

G_DEFINE_TYPE (CcDisplaySettings, cc_display_settings, GTK_TYPE_BOX)

static GParamSpec *props[PROP_LAST];

static void on_scale_btn_active_changed_cb (CcDisplaySettings *self,
                                            GParamSpec        *pspec,
                                            GtkWidget         *widget);

static gboolean
should_show_rotation (CcDisplaySettings *self)
{
  gboolean supports_rotation;

  supports_rotation = cc_display_monitor_supports_rotation (self->selected_output,
                                                            CC_DISPLAY_ROTATION_90 |
                                                            CC_DISPLAY_ROTATION_180 |
                                                            CC_DISPLAY_ROTATION_270);

  /* Doesn't support rotation at all */
  if (!supports_rotation)
    return FALSE;

  /* We can always rotate displays that aren't builtin */
  if (!cc_display_monitor_is_builtin (self->selected_output))
    return TRUE;

  /* Only offer rotation if there's no accelerometer */
  return !self->has_accelerometer;
}

static const gchar *
string_for_rotation (CcDisplayRotation rotation)
{
  switch (rotation)
    {
    case CC_DISPLAY_ROTATION_NONE:
    case CC_DISPLAY_ROTATION_180_FLIPPED:
      return C_("Display rotation", "Landscape");
    case CC_DISPLAY_ROTATION_90:
    case CC_DISPLAY_ROTATION_270_FLIPPED:
      return C_("Display rotation", "Portrait Right");
    case CC_DISPLAY_ROTATION_270:
    case CC_DISPLAY_ROTATION_90_FLIPPED:
      return C_("Display rotation", "Portrait Left");
    case CC_DISPLAY_ROTATION_180:
    case CC_DISPLAY_ROTATION_FLIPPED:
      return C_("Display rotation", "Landscape (flipped)");
    }
  return "";
}

static const gchar *
make_aspect_string (gint width,
                    gint height)
{
  int ratio;
  const gchar *aspect = NULL;

    /* We use a number of Unicode characters below:
     * ∶ is U+2236 RATIO
     *   is U+2009 THIN SPACE,
     * × is U+00D7 MULTIPLICATION SIGN
     */
  if (width && height) {
    if (width > height)
      ratio = width * 10 / height;
    else
      ratio = height * 10 / width;

    switch (ratio) {
    case 13:
      aspect = "4∶3";
      break;
    case 16:
      aspect = "16∶10";
      break;
    case 17:
      aspect = "16∶9";
      break;
    case 23:
      aspect = "21∶9";
      break;
    case 35:
      aspect = "32∶9";
      break;
    case 12:
      aspect = "5∶4";
      break;
      /* This catches 1.5625 as well (1600x1024) when maybe it shouldn't. */
    case 15:
      aspect = "3∶2";
      break;
    case 18:
      aspect = "9∶5";
      break;
    case 10:
      aspect = "1∶1";
      break;
    }
  }

  return aspect;
}

static gchar *
make_refresh_rate_string (CcDisplayMode *mode)
{
  return g_strdup_printf (_("%.2lf Hz"), cc_display_mode_get_freq_f (mode));
}

static gchar *
make_variable_refresh_rate_string (CcDisplayMonitor *output,
                                   CcDisplayMode    *mode)
{
  int min_freq;

  min_freq = cc_display_monitor_get_min_freq (output);
  if (min_freq > 0)
    {
      /* Translators:
       * 1. "Variable" is an adjective that refers to the refresh rate
       * 2. The formatting sequence is a range separated by an en-dash
       *    (unicode "\u2013"). For example: "Variable (48–144.97 Hz)"
       */
      return g_strdup_printf (_("Variable (%d\u2013%.2lf Hz)"),
                              min_freq,
                              cc_display_mode_get_freq_f (mode));
    }
  else
    {
      /* Translators: "Variable" is an adjective that refers to the refresh rate */
      return g_strdup_printf (_("Variable (up to %.2lf Hz)"),
                              cc_display_mode_get_freq_f (mode));
    }
}

static gchar *
make_expander_refresh_rate_string (CcDisplayMonitor *output,
                                   CcDisplayMode    *mode)
{
  switch (cc_display_mode_get_refresh_rate_mode (mode))
    {
    case MODE_REFRESH_RATE_MODE_FIXED:
      return make_refresh_rate_string (mode);
    case MODE_REFRESH_RATE_MODE_VARIABLE:
      return make_variable_refresh_rate_string (output, mode);
    default:
      g_assert_not_reached();
    }

  return NULL;
}

static gboolean
mode_to_refresh_rate_transform_func (GBinding          *binding,
                                     const GValue      *source_value,
                                     GValue            *target_value,
                                     CcDisplaySettings *self)
{
  CcDisplayMode *mode;
  gchar *refresh_rate_string;

  if (!G_VALUE_HOLDS_OBJECT (source_value))
    return FALSE;

  if (!G_VALUE_HOLDS_STRING (target_value))
    return FALSE;

  mode = CC_DISPLAY_MODE (g_value_get_object (source_value));
  g_return_val_if_fail (mode != NULL, FALSE);

  refresh_rate_string =
    make_expander_refresh_rate_string (self->selected_output, mode);

  g_value_take_string (target_value, refresh_rate_string);

  return TRUE;
}

static gchar *
make_resolution_string (CcDisplayMode *mode)
{
  const char *interlaced;
  const char *aspect;
  int width, height;

  cc_display_mode_get_resolution (mode, &width, &height);
  aspect = make_aspect_string (width, height);
  interlaced = cc_display_mode_is_interlaced (mode) ? "i" : "";

  if (aspect != NULL)
    return g_strdup_printf ("%d × %d%s (%s)", width, height, interlaced, aspect);
  else
    return g_strdup_printf ("%d × %d%s", width, height, interlaced);
}

static double
round_scale_for_ui (double scale)
{
  /* Keep in sync with mutter */
  return round (scale*4)/4;
}

static gchar *
make_scale_string (gdouble scale)
{
  return g_strdup_printf ("%d %%", (int) (round_scale_for_ui (scale)*100));
}

static gint
sort_modes_by_area_desc (CcDisplayMode *a, CcDisplayMode *b)
{
  gint wa, ha, wb, hb;
  gint res;

  cc_display_mode_get_resolution (a, &wa, &ha);
  cc_display_mode_get_resolution (b, &wb, &hb);

  /* Sort first by width, then height.
   * We used to sort by area, but that can be confusing. */
  res = wb - wa;
  if (res)
    return res;
  return hb - ha;
}

static gint
sort_modes_by_refresh_rate_desc (CcDisplayMode *a, CcDisplayMode *b)
{
  if (cc_display_mode_get_refresh_rate_mode (a) != cc_display_mode_get_refresh_rate_mode (b))
    {
      if (cc_display_mode_get_refresh_rate_mode (a) == MODE_REFRESH_RATE_MODE_VARIABLE)
        return -1;
      else
        return 1;
    }

  double delta = (cc_display_mode_get_freq_f (b) - cc_display_mode_get_freq_f (a))*1000.;

  return delta;
}

static gboolean
cc_display_settings_rebuild_ui (CcDisplaySettings *self)
{
  GtkWidget *child;
  g_autolist(CcDisplayMode) clone_modes = NULL;
  GList *modes;
  GList *item;
  gint width, height;
  CcDisplayMode *current_mode;
  GtkToggleButton *group = NULL;
  g_autoptr(GArray) scales = NULL;
  gint i;

  self->idle_udpate_id = 0;

  if (!self->config || !self->selected_output)
    {
      gtk_widget_set_visible (self->enabled_listbox, FALSE);
      gtk_widget_set_visible (self->orientation_row, FALSE);
      gtk_widget_set_visible (self->refresh_rate_row, FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->refresh_rate_expander_row), FALSE);
      gtk_widget_set_visible (self->resolution_row, FALSE);
      gtk_widget_set_visible (self->scale_combo_row, FALSE);
      gtk_widget_set_visible (self->scale_buttons_row, FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->underscanning_row), FALSE);

      return G_SOURCE_REMOVE;
    }

  g_object_freeze_notify ((GObject*) self->enabled_row);
  g_object_freeze_notify ((GObject*) self->orientation_row);
  g_object_freeze_notify ((GObject*) self->refresh_rate_row);
  g_object_freeze_notify ((GObject*) self->refresh_rate_expander_row);
  g_object_freeze_notify ((GObject*) self->variable_refresh_rate_row);
  g_object_freeze_notify ((GObject*) self->preferred_refresh_rate_row);
  g_object_freeze_notify ((GObject*) self->resolution_row);
  g_object_freeze_notify ((GObject*) self->scale_combo_row);
  g_object_freeze_notify ((GObject*) self->underscanning_row);

  cc_display_monitor_get_geometry (self->selected_output, NULL, NULL, &width, &height);

  /* Selecte the first mode we can find if the monitor is disabled. */
  current_mode = cc_display_monitor_get_mode (self->selected_output);
  if (current_mode == NULL)
    current_mode = cc_display_monitor_get_preferred_mode (self->selected_output);
  if (current_mode == NULL) {
    modes = cc_display_monitor_get_modes (self->selected_output);
    /* Lets assume that a monitor always has at least one mode. */
    g_assert (modes);
    current_mode = CC_DISPLAY_MODE (modes->data);
  }

  /* Enabled Switch */
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->enabled_row),
                                 cc_display_monitor_get_ui_name (self->selected_output));
  adw_switch_row_set_active (self->enabled_row,
                             cc_display_monitor_is_active (self->selected_output));

  if (should_show_rotation (self))
    {
      guint i;
      CcDisplayRotation rotations[] = { CC_DISPLAY_ROTATION_NONE,
                                        CC_DISPLAY_ROTATION_90,
                                        CC_DISPLAY_ROTATION_270,
                                        CC_DISPLAY_ROTATION_180 };

      gtk_widget_set_visible (self->orientation_row, TRUE);

      gtk_string_list_splice (GTK_STRING_LIST (self->orientation_list),
                              0,
                              g_list_model_get_n_items (self->orientation_list),
                              NULL);
      for (i = 0; i < G_N_ELEMENTS (rotations); i++)
        {
          g_autoptr(GObject) obj = NULL;

          if (!cc_display_monitor_supports_rotation (self->selected_output, rotations[i]))
            continue;

          gtk_string_list_append (GTK_STRING_LIST (self->orientation_list),
                                  string_for_rotation (rotations[i]));
          obj = g_list_model_get_item (self->orientation_list, i);
          g_object_set_data (G_OBJECT (obj), "rotation-value", GINT_TO_POINTER (rotations[i]));

          if (cc_display_monitor_get_rotation (self->selected_output) == rotations[i])
            adw_combo_row_set_selected (ADW_COMBO_ROW (self->orientation_row),
                                        g_list_model_get_n_items (G_LIST_MODEL (self->orientation_list)) - 1);
        }
    }
  else
    {
      gtk_widget_set_visible (self->orientation_row, FALSE);
    }

  /* Only show refresh rate if we are not in cloning mode. */
  if (!cc_display_config_is_cloning (self->config))
    {
      GList *item;
      gdouble current_freq;
      CcDisplayModeRefreshRateMode current_refresh_rate_mode;
      gboolean has_variable_refresh_rate_modes = FALSE;

      current_freq = cc_display_mode_get_freq_f (current_mode);
      current_refresh_rate_mode = cc_display_mode_get_refresh_rate_mode (current_mode);

      modes = cc_display_monitor_get_modes (self->selected_output);

      g_list_store_remove_all (self->refresh_rate_list);

      for (item = modes; item != NULL; item = item->next)
        {
          gint w, h;
          guint new;
          CcDisplayMode *mode = CC_DISPLAY_MODE (item->data);
          CcDisplayModeRefreshRateMode refresh_rate_mode;

          cc_display_mode_get_resolution (mode, &w, &h);
          if (w != width || h != height)
            continue;

          refresh_rate_mode = cc_display_mode_get_refresh_rate_mode (mode);

          if (refresh_rate_mode == MODE_REFRESH_RATE_MODE_VARIABLE)
            has_variable_refresh_rate_modes = TRUE;

          if (current_refresh_rate_mode != refresh_rate_mode)
            continue;

          /* At some point we used to filter very close resolutions,
           * but we don't anymore these days.
           */
          new = g_list_store_insert_sorted (self->refresh_rate_list,
                                            mode,
                                            (GCompareDataFunc) sort_modes_by_refresh_rate_desc,
                                            NULL);

          if (current_freq != cc_display_mode_get_freq_f (mode))
            continue;

          adw_combo_row_set_selected (ADW_COMBO_ROW (self->refresh_rate_row), new);
          adw_combo_row_set_selected (self->preferred_refresh_rate_row, new);
        }

      adw_switch_row_set_active (self->variable_refresh_rate_row,
                                 current_refresh_rate_mode == MODE_REFRESH_RATE_MODE_VARIABLE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->variable_refresh_rate_row),
                                has_variable_refresh_rate_modes);

      if (cc_display_monitor_supports_variable_refresh_rate (self->selected_output))
        {
          gtk_widget_set_visible (self->refresh_rate_row, FALSE);
          gtk_widget_set_visible (GTK_WIDGET (self->refresh_rate_expander_row), TRUE);
        }
      else
        {
          gtk_widget_set_visible (self->refresh_rate_row, TRUE);
          gtk_widget_set_visible (GTK_WIDGET (self->refresh_rate_expander_row), FALSE);
        }
    }
  else
    {
      gtk_widget_set_visible (self->refresh_rate_row, FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->refresh_rate_expander_row), FALSE);
    }


  /* Resolutions are always shown. */
  gtk_widget_set_visible (self->resolution_row, TRUE);
  if (cc_display_config_is_cloning (self->config))
    {
      clone_modes = cc_display_config_generate_cloning_modes (self->config);
      modes = clone_modes;
    }
  else
    {
      modes = cc_display_monitor_get_modes (self->selected_output);
    }

  g_list_store_remove_all (self->resolution_list);
  g_list_store_append (self->resolution_list, current_mode);
  adw_combo_row_set_selected (ADW_COMBO_ROW (self->resolution_row), 0);
  for (item = modes; item != NULL; item = item->next)
    {
      gint ins;
      gint w, h;
      CcDisplayMode *mode = CC_DISPLAY_MODE (item->data);

      cc_display_mode_get_resolution (mode, &w, &h);

      /* Find the appropriate insertion point. */
      for (ins = 0; ins < g_list_model_get_n_items (G_LIST_MODEL (self->resolution_list)); ins++)
        {
          g_autoptr(CcDisplayMode) m = NULL;
          gint cmp;

          m = g_list_model_get_item (G_LIST_MODEL (self->resolution_list), ins);

          cmp = sort_modes_by_area_desc (mode, m);
          /* Next item is smaller, insert at this point. */
          if (cmp < 0)
            break;

          /* Don't insert if it is already in the list */
          if (cmp == 0)
            {
              ins = -1;
              break;
            }
        }

        if (ins >= 0)
          g_list_store_insert (self->resolution_list, ins, mode);
    }


  /* Scale row is usually shown. */
  while ((child = gtk_widget_get_first_child (self->scale_bbox)) != NULL)
    gtk_box_remove (GTK_BOX (self->scale_bbox), child);

  gtk_string_list_splice (GTK_STRING_LIST (self->scale_list),
                          0,
                          g_list_model_get_n_items (self->scale_list),
                          NULL);
  scales = cc_display_mode_get_supported_scales (current_mode);
  self->num_scales = scales->len;
  for (i = 0; i < scales->len; i++)
    {
      g_autofree gchar *scale_str = NULL;
      g_autoptr(GObject) value_object = NULL;
      double scale = g_array_index (scales, double, i);
      GtkWidget *scale_btn;
      gboolean is_selected;

      /* ComboRow */
      scale_str = make_scale_string (scale);
      is_selected = G_APPROX_VALUE (cc_display_monitor_get_scale (self->selected_output),
                                    scale, DBL_EPSILON);

      gtk_string_list_append (GTK_STRING_LIST (self->scale_list), scale_str);
      value_object = g_list_model_get_item (self->scale_list, i);
      g_object_set_data_full (G_OBJECT (value_object), "scale",
                              g_memdup2 (&scale, sizeof (double)), g_free);
      if (is_selected)
        adw_combo_row_set_selected (ADW_COMBO_ROW (self->scale_combo_row),
                                    g_list_model_get_n_items (G_LIST_MODEL (self->scale_list)) - 1);

      /* ButtonBox */
      scale_btn = gtk_toggle_button_new_with_label (scale_str);
      gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (scale_btn), group);
      g_object_set_data_full (G_OBJECT (scale_btn), "scale",
                              g_memdup2 (&scale, sizeof (double)), g_free);

      if (!group)
        group = GTK_TOGGLE_BUTTON (scale_btn);
      gtk_box_append (GTK_BOX (self->scale_bbox), scale_btn);
      /* Set active before connecting the signal */
      if (is_selected)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (scale_btn), TRUE);

      g_signal_connect_object (scale_btn,
                               "notify::active",
                               G_CALLBACK (on_scale_btn_active_changed_cb),
                               self, G_CONNECT_SWAPPED);
    }
  cc_display_settings_refresh_layout (self, self->collapsed);

  gtk_widget_set_visible (GTK_WIDGET (self->underscanning_row),
                          cc_display_monitor_supports_underscanning (self->selected_output) &&
                          !cc_display_config_is_cloning (self->config));
  adw_switch_row_set_active (self->underscanning_row,
                             cc_display_monitor_get_underscanning (self->selected_output));

  self->updating = TRUE;
  g_object_thaw_notify ((GObject*) self->enabled_row);
  g_object_thaw_notify ((GObject*) self->orientation_row);
  g_object_thaw_notify ((GObject*) self->refresh_rate_row);
  g_object_thaw_notify ((GObject*) self->refresh_rate_expander_row);
  g_object_thaw_notify ((GObject*) self->variable_refresh_rate_row);
  g_object_thaw_notify ((GObject*) self->preferred_refresh_rate_row);
  g_object_thaw_notify ((GObject*) self->resolution_row);
  g_object_thaw_notify ((GObject*) self->scale_combo_row);
  g_object_thaw_notify ((GObject*) self->underscanning_row);
  self->updating = FALSE;

  return G_SOURCE_REMOVE;
}

static void
on_output_changed_cb (CcDisplaySettings *self,
                      GParamSpec        *pspec,
                      CcDisplayMonitor  *output)
{
  /* Do this frmo an idle handler, because otherwise we may create an
   * infinite loop triggering the notify::selected-index from the
   * combo rows. */
  if (self->idle_udpate_id)
    return;

  self->idle_udpate_id = g_idle_add ((GSourceFunc) cc_display_settings_rebuild_ui, self);
}

static void
on_enabled_row_active_changed_cb (CcDisplaySettings *self)
{
  if (self->updating)
    return;

  cc_display_monitor_set_active (self->selected_output,
                                 adw_switch_row_get_active (self->enabled_row));

  g_signal_emit_by_name (G_OBJECT (self), "updated", self->selected_output);
}

static void
on_orientation_selection_changed_cb (CcDisplaySettings *self)
{
  gint idx;
  g_autoptr(GObject) obj = NULL;

  if (self->updating)
    return;

  idx = adw_combo_row_get_selected (ADW_COMBO_ROW (self->orientation_row));
  obj = g_list_model_get_item (G_LIST_MODEL (self->orientation_list), idx);

  if (!obj)
    return;

  cc_display_monitor_set_rotation (self->selected_output,
                                   GPOINTER_TO_INT (g_object_get_data (G_OBJECT (obj), "rotation-value")));

  g_signal_emit_by_name (G_OBJECT (self), "updated", self->selected_output);
}

static void
on_refresh_rate_selection_changed_cb (CcDisplaySettings *self)
{
  gint idx;
  g_autoptr(CcDisplayMode) mode = NULL;

  if (self->updating)
    return;

  idx = adw_combo_row_get_selected (ADW_COMBO_ROW (self->refresh_rate_row));
  mode = g_list_model_get_item (G_LIST_MODEL (self->refresh_rate_list), idx);

  if (!mode)
    return;

  cc_display_monitor_set_mode (self->selected_output, mode);

  g_signal_emit_by_name (G_OBJECT (self), "updated", self->selected_output);
}

static void
on_variable_refresh_rate_active_changed_cb (CcDisplaySettings *self)
{
  if (self->updating)
    return;

  if (!adw_switch_row_get_active (self->variable_refresh_rate_row))
    {
      cc_display_monitor_set_refresh_rate_mode (self->selected_output,
                                                MODE_REFRESH_RATE_MODE_FIXED);
    }
  else
    {
      cc_display_monitor_set_refresh_rate_mode (self->selected_output,
                                                MODE_REFRESH_RATE_MODE_VARIABLE);
    }

  g_signal_emit_by_name (G_OBJECT (self), "updated", self->selected_output);
}

static void
on_resolution_selection_changed_cb (CcDisplaySettings *self)
{
  gint idx;
  g_autoptr(CcDisplayMode) mode = NULL;

  if (self->updating)
    return;

  idx = adw_combo_row_get_selected (ADW_COMBO_ROW (self->resolution_row));
  mode = g_list_model_get_item (G_LIST_MODEL (self->resolution_list), idx);

  if (!mode)
    return;

  /* This is the only row that can be changed when in cloning mode. */
  if (!cc_display_config_is_cloning (self->config))
    cc_display_monitor_set_mode (self->selected_output, mode);
  else
    cc_display_config_set_mode_on_all_outputs (self->config, mode);

  g_signal_emit_by_name (G_OBJECT (self), "updated", self->selected_output);
}

static void
on_scale_btn_active_changed_cb (CcDisplaySettings *self,
                                GParamSpec        *pspec,
                                GtkWidget         *widget)
{
  gdouble scale;
  if (self->updating)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  scale = *(gdouble*) g_object_get_data (G_OBJECT (widget), "scale");
  cc_display_monitor_set_scale (self->selected_output,
                                scale);

  g_signal_emit_by_name (G_OBJECT (self), "updated", self->selected_output);
}

static void
on_scale_selection_changed_cb (CcDisplaySettings *self)
{
  int idx;
  double scale;
  g_autoptr(GObject) obj = NULL;

  if (self->updating)
    return;

  idx = adw_combo_row_get_selected (ADW_COMBO_ROW (self->scale_combo_row));
  obj = g_list_model_get_item (G_LIST_MODEL (self->scale_list), idx);
  if (!obj)
    return;
  scale = *(gdouble*) g_object_get_data (G_OBJECT (obj), "scale");

  cc_display_monitor_set_scale (self->selected_output, scale);

  g_signal_emit_by_name (G_OBJECT (self), "updated", self->selected_output);
}

static void
on_underscanning_row_active_changed_cb (CcDisplaySettings *self)
{
  if (self->updating)
    return;

  cc_display_monitor_set_underscanning (self->selected_output,
                                        adw_switch_row_get_active (self->underscanning_row));

  g_signal_emit_by_name (G_OBJECT (self), "updated", self->selected_output);
}

static void
cc_display_settings_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CcDisplaySettings *self = CC_DISPLAY_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_HAS_ACCELEROMETER:
      g_value_set_boolean (value, cc_display_settings_get_has_accelerometer (self));
      break;

    case PROP_CONFIG:
      g_value_set_object (value, self->config);
      break;

    case PROP_SELECTED_OUTPUT:
      g_value_set_object (value, self->selected_output);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_display_settings_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  CcDisplaySettings *self = CC_DISPLAY_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_HAS_ACCELEROMETER:
      cc_display_settings_set_has_accelerometer (self, g_value_get_boolean (value));
      break;

    case PROP_CONFIG:
      cc_display_settings_set_config (self, g_value_get_object (value));
      break;

    case PROP_SELECTED_OUTPUT:
      cc_display_settings_set_selected_output (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_display_settings_finalize (GObject *object)
{
  CcDisplaySettings *self = CC_DISPLAY_SETTINGS (object);

  g_clear_object (&self->config);

  g_clear_object (&self->orientation_list);
  g_clear_object (&self->refresh_rate_list);
  g_clear_object (&self->resolution_list);
  g_clear_object (&self->scale_list);

  g_clear_handle_id (&self->idle_udpate_id, g_source_remove);

  G_OBJECT_CLASS (cc_display_settings_parent_class)->finalize (object);
}

static void
cc_display_settings_class_init (CcDisplaySettingsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->finalize = cc_display_settings_finalize;
  gobject_class->get_property = cc_display_settings_get_property;
  gobject_class->set_property = cc_display_settings_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/display/cc-display-settings.ui");

  props[PROP_HAS_ACCELEROMETER] =
    g_param_spec_boolean ("has-accelerometer", "Has Accelerometer",
                          "If an accelerometre is available for the builtin display",
                          FALSE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_CONFIG] =
    g_param_spec_object ("config", "Display Config",
                         "The display configuration to work with",
                         CC_TYPE_DISPLAY_CONFIG,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_SELECTED_OUTPUT] =
    g_param_spec_object ("selected-output", "Selected Output",
                         "The output that is currently selected on the configuration",
                         CC_TYPE_DISPLAY_MONITOR,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     props);

  g_signal_new ("updated",
                CC_TYPE_DISPLAY_SETTINGS,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 1, CC_TYPE_DISPLAY_MONITOR);

  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, enabled_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, enabled_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, orientation_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, refresh_rate_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, refresh_rate_expander_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, refresh_rate_expander_suffix_label);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, variable_refresh_rate_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, preferred_refresh_rate_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, resolution_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, scale_bbox);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, scale_buttons_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, scale_combo_row);
  gtk_widget_class_bind_template_child (widget_class, CcDisplaySettings, underscanning_row);

  gtk_widget_class_bind_template_callback (widget_class, on_enabled_row_active_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_orientation_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_refresh_rate_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_variable_refresh_rate_active_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_resolution_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scale_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_underscanning_row_active_changed_cb);
}

static void
cc_display_settings_init (CcDisplaySettings *self)
{
  GtkExpression *expression;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->orientation_list = G_LIST_MODEL (gtk_string_list_new (NULL));
  self->refresh_rate_list = g_list_store_new (CC_TYPE_DISPLAY_MODE);
  self->resolution_list = g_list_store_new (CC_TYPE_DISPLAY_MODE);
  self->scale_list = G_LIST_MODEL (gtk_string_list_new (NULL));

  self->updating = TRUE;

  adw_combo_row_set_model (ADW_COMBO_ROW (self->orientation_row),
                           G_LIST_MODEL (self->orientation_list));
  adw_combo_row_set_model (ADW_COMBO_ROW (self->scale_combo_row),
                           G_LIST_MODEL (self->scale_list));

  expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                            NULL, 0, NULL,
                                            G_CALLBACK (make_refresh_rate_string),
                                            self, NULL);
  adw_combo_row_set_expression (ADW_COMBO_ROW (self->refresh_rate_row), expression);
  adw_combo_row_set_model (ADW_COMBO_ROW (self->refresh_rate_row),
                           G_LIST_MODEL (self->refresh_rate_list));

  adw_combo_row_set_expression (self->preferred_refresh_rate_row, expression);
  adw_combo_row_set_model (self->preferred_refresh_rate_row,
                           G_LIST_MODEL (self->refresh_rate_list));

  g_object_bind_property_full (self->preferred_refresh_rate_row,
                               "selected-item",
                               self->refresh_rate_expander_suffix_label,
                               "label",
                               G_BINDING_DEFAULT,
                               (GBindingTransformFunc) mode_to_refresh_rate_transform_func,
                               NULL, self, NULL);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING,
                                            NULL, 0, NULL,
                                            G_CALLBACK (make_resolution_string),
                                            self, NULL);
  adw_combo_row_set_expression (ADW_COMBO_ROW (self->resolution_row), expression);
  adw_combo_row_set_model (ADW_COMBO_ROW (self->resolution_row),
                           G_LIST_MODEL (self->resolution_list));

  self->updating = FALSE;
}

CcDisplaySettings*
cc_display_settings_new (void)
{
  return g_object_new (CC_TYPE_DISPLAY_SETTINGS,
                       NULL);
}

gboolean
cc_display_settings_get_has_accelerometer (CcDisplaySettings    *settings)
{
  return settings->has_accelerometer;
}

void
cc_display_settings_set_has_accelerometer (CcDisplaySettings    *self,
                                           gboolean              has_accelerometer)
{
  self->has_accelerometer = has_accelerometer;

  cc_display_settings_rebuild_ui (self);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONFIG]);
}

CcDisplayConfig*
cc_display_settings_get_config (CcDisplaySettings *self)
{
  return self->config;
}

void
cc_display_settings_set_config (CcDisplaySettings *self,
                                CcDisplayConfig   *config)
{
  const gchar *signals[] = { "rotation", "mode", "scale", "is-usable", "active" };
  GList *outputs, *l;
  guint i;

  if (self->config)
    {
      outputs = cc_display_config_get_monitors (self->config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;

          g_signal_handlers_disconnect_by_data (output, self);
        }
    }
  g_clear_object (&self->config);

  self->config = g_object_ref (config);

  /* Listen to all the signals */
  if (self->config)
    {
      outputs = cc_display_config_get_monitors (self->config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;

          for (i = 0; i < G_N_ELEMENTS (signals); ++i)
            g_signal_connect_object (output, signals[i], G_CALLBACK (on_output_changed_cb), self, G_CONNECT_SWAPPED);
        }
    }

  cc_display_settings_set_selected_output (self, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONFIG]);
}

CcDisplayMonitor*
cc_display_settings_get_selected_output (CcDisplaySettings *self)
{
  return self->selected_output;
}

void
cc_display_settings_set_selected_output (CcDisplaySettings *self,
                                         CcDisplayMonitor  *output)
{
  self->selected_output = output;

  adw_expander_row_set_expanded (self->refresh_rate_expander_row, FALSE);

  cc_display_settings_rebuild_ui (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_OUTPUT]);
}

void
cc_display_settings_refresh_layout (CcDisplaySettings *self,
                                    gboolean           collapsed)
{
  gboolean use_combo;

  self->collapsed = collapsed;
  use_combo = self->num_scales > MAX_SCALE_BUTTONS || (self->num_scales > 2 && collapsed);

  gtk_widget_set_visible (self->scale_combo_row, use_combo);
  gtk_widget_set_visible (self->scale_buttons_row, self->num_scales > 1 && !use_combo);
}

void
cc_display_settings_set_multimonitor (CcDisplaySettings *self,
                                      gboolean           multimonitor)
{
  gtk_widget_set_visible (self->enabled_listbox, multimonitor);

  if (!multimonitor)
    adw_switch_row_set_active (self->enabled_row, TRUE);
}
