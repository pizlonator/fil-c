/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
 * Copyright (c) 2016 Endless, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#define G_LOG_DOMAIN "cc-window"

#include <config.h>

#include "cc-log.h"
#include "cc-window.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>

#include "cc-application.h"
#include "cc-panel.h"
#include "cc-shell.h"
#include "cc-shell-model.h"
#include "cc-panel-list.h"
#include "cc-panel-loader.h"
#include "cc-util.h"

#define MOUSE_BACK_BUTTON 8

#define DEFAULT_WINDOW_ICON_NAME "gnome-control-center"

struct _CcWindow
{
  AdwApplicationWindow parent;

  AdwMessageDialog  *development_warning_dialog;
  AdwNavigationSplitView *split_view;
  CcPanelList       *panel_list;
  GtkSearchBar      *search_bar;
  GtkSearchEntry    *search_entry;

  GtkWidget  *old_panel;
  GtkWidget  *current_panel;
  char       *current_panel_id;
  GQueue     *previous_panels;

  GtkWidget  *custom_titlebar;

  CcShellModel *store;

  CcPanel *active_panel;
  GSettings *settings;

  CcPanelListView previous_list_view;
};

static void     cc_shell_iface_init         (CcShellInterface      *iface);

G_DEFINE_TYPE_WITH_CODE (CcWindow, cc_window, ADW_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (CC_TYPE_SHELL, cc_shell_iface_init))

enum
{
  PROP_0,
  PROP_ACTIVE_PANEL,
  PROP_MODEL,
  PROP_COLLAPSED,
};

/* Auxiliary methods */
static void
load_window_state (CcWindow *self)
{
  gint current_width = -1;
  gint current_height = -1;
  gboolean maximized = FALSE;

  g_settings_get (self->settings,
                 "window-state",
                 "(iib)",
                 &current_width,
                 &current_height,
                 &maximized);

  if (current_width != -1 && current_height != -1)
    gtk_window_set_default_size (GTK_WINDOW (self), current_width, current_height);
  if (maximized)
    gtk_window_maximize (GTK_WINDOW (self));
}

static gboolean
in_flatpak_sandbox (void)
{
  return g_strcmp0 (PROFILE, "development") == 0;
}

static gboolean
activate_panel (CcWindow          *self,
                const gchar       *id,
                GVariant          *parameters,
                const gchar       *name,
                GIcon             *gicon,
                CcPanelVisibility  visibility)
{
  g_autoptr(GTimer) timer = NULL;
  gdouble elapsed_time;

  CC_ENTRY;

  if (!id)
    CC_RETURN (FALSE);

  if (visibility == CC_PANEL_HIDDEN)
    CC_RETURN (FALSE);

  timer = g_timer_new ();

  /* Begin the profile */
  g_timer_start (timer);

  if (self->current_panel)
    g_signal_handlers_disconnect_by_data (self->current_panel, self);
  self->current_panel = GTK_WIDGET (cc_panel_loader_load_by_name (CC_SHELL (self), id, name, parameters));
  cc_shell_set_active_panel (CC_SHELL (self), CC_PANEL (self->current_panel));

  adw_navigation_split_view_set_content (self->split_view, ADW_NAVIGATION_PAGE (self->current_panel));

  /* Finish profiling */
  g_timer_stop (timer);

  elapsed_time = g_timer_elapsed (timer, NULL);

  g_debug ("Time to open panel '%s': %lfs", name, elapsed_time);

  g_settings_set_string (self->settings, "last-panel", id);

  CC_RETURN (TRUE);
}

static void
add_current_panel_to_history (CcWindow   *self,
                              const char *start_id)
{
  g_return_if_fail (start_id != NULL);

  if (!self->current_panel_id || g_strcmp0 (self->current_panel_id, start_id) == 0)
    return;

  g_queue_push_head (self->previous_panels, g_strdup (self->current_panel_id));
  g_debug ("Added '%s' to the previous panels", self->current_panel_id);
}

static gboolean
find_iter_for_panel_id (CcWindow    *self,
                        const gchar *panel_id,
                        GtkTreeIter *out_iter)
{
  GtkTreeIter iter;
  gboolean valid;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter);

  while (valid)
    {
      g_autofree gchar *id = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (self->store),
                          &iter,
                          COL_ID, &id,
                          -1);

      if (g_strcmp0 (id, panel_id) == 0)
        break;

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->store), &iter);
    }

  g_assert (out_iter != NULL);
  *out_iter = iter;

  return valid;
}

static void
on_row_changed_cb (CcWindow     *self,
                   GtkTreePath  *path,
                   GtkTreeIter  *iter,
                   GtkTreeModel *model)
{
  g_autofree gchar *id = NULL;
  CcPanelVisibility visibility;

  gtk_tree_model_get (model, iter,
                      COL_ID, &id,
                      COL_VISIBILITY, &visibility,
                      -1);

  cc_panel_list_set_panel_visibility (self->panel_list, id, visibility);
}

static void
setup_model (CcWindow *self)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;

  /* CcApplication must have a valid model at this point */
  g_assert (self->store != NULL);

  model = GTK_TREE_MODEL (self->store);

  cc_panel_loader_fill_model (self->store);

  /* Create a row for each panel */
  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      CcPanelCategory category;
      g_autoptr(GIcon) icon = NULL;
      g_autofree gchar *name = NULL;
      g_autofree gchar *description = NULL;
      g_autofree gchar *id = NULL;
      g_auto(GStrv) keywords = NULL;
      CcPanelVisibility visibility;
      const gchar *icon_name = NULL;

      gtk_tree_model_get (model, &iter,
                          COL_CATEGORY, &category,
                          COL_DESCRIPTION, &description,
                          COL_GICON, &icon,
                          COL_ID, &id,
                          COL_NAME, &name,
                          COL_KEYWORDS, &keywords,
                          COL_VISIBILITY, &visibility,
                          -1);

      if (G_IS_THEMED_ICON (icon))
        icon_name = g_themed_icon_get_names (G_THEMED_ICON (icon))[0];

      cc_panel_list_add_panel (self->panel_list,
                               category,
                               id,
                               name,
                               description,
                               keywords,
                               icon_name,
                               visibility);

      valid = gtk_tree_model_iter_next (model, &iter);
    }

  /* React to visibility changes */
  g_signal_connect_object (model, "row-changed", G_CALLBACK (on_row_changed_cb), self, G_CONNECT_SWAPPED);
}

static gboolean
set_active_panel_from_id (CcWindow     *self,
                          const gchar  *start_id,
                          GVariant     *parameters,
                          gboolean      add_to_history,
                          gboolean      force_moving_to_the_panel,
                          GError      **error)
{
  g_autoptr(GIcon) gicon = NULL;
  g_autofree gchar *name = NULL;
  CcPanelVisibility visibility;
  CcPanelCategory category;
  g_autoptr(GVariant) system_param_overwrite = NULL;
  GtkTreeIter iter;
  CcPanelListView view;
  gboolean activated;
  gboolean found;

  CC_ENTRY;

  view = cc_panel_list_get_view (self->panel_list);

  /* When loading the same panel again, just set its parameters */
  if (g_strcmp0 (self->current_panel_id, start_id) == 0)
    {
      g_object_set (G_OBJECT (self->current_panel), "parameters", parameters, NULL);
      if (force_moving_to_the_panel || self->previous_list_view == view)
        adw_navigation_split_view_set_show_content (self->split_view, TRUE);
      self->previous_list_view = view;
      CC_RETURN (TRUE);
    }

  found = find_iter_for_panel_id (self, start_id, &iter);
  if (!found)
    {
      g_warning ("Could not find settings panel \"%s\"", start_id);
      CC_RETURN (TRUE);
    }

  self->old_panel = self->current_panel;
  if (self->old_panel)
    cc_panel_deactivate (CC_PANEL (self->old_panel));

  gtk_tree_model_get (GTK_TREE_MODEL (self->store),
                      &iter,
                      COL_NAME, &name,
                      COL_GICON, &gicon,
                      COL_VISIBILITY, &visibility,
                      COL_CATEGORY, &category,
                      -1);

  /* Handle "System" subpages by overwriting the start_id and parameters arguments.
   * This is needed because they have their own desktop files. */
  if (category == CC_CATEGORY_SYSTEM)
    {
      g_autofree gchar *param_str = NULL;

      param_str = g_strdup_printf ("[<'%s'>]", start_id);
      system_param_overwrite = g_variant_new_parsed (param_str);
      g_variant_ref_sink (system_param_overwrite);

      g_warning ("The direct access to `%s` is now deprecated. Please, use `system %s` instead.", start_id, start_id);

      start_id = "system";
    }

  /* Activate the panel */
  activated = activate_panel (self,
                              start_id,
                              system_param_overwrite != NULL ? system_param_overwrite : parameters,
                              name,
                              gicon,
                              visibility);

  /* Failed to activate the panel for some reason, let's keep the old
   * panel around instead */
  if (!activated)
    {
      g_debug ("Failed to activate panel");
      CC_RETURN (TRUE);
    }

  if (add_to_history)
    add_current_panel_to_history (self, start_id);

  if (force_moving_to_the_panel)
    adw_navigation_split_view_set_show_content (self->split_view, TRUE);

  g_free (self->current_panel_id);
  self->current_panel_id = g_strdup (start_id);

  CC_TRACE_MSG ("Current panel id: %s", start_id);

  cc_panel_list_set_active_panel (self->panel_list, start_id);

  CC_RETURN (TRUE);
}

static void
set_active_panel (CcWindow *self,
                  CcPanel  *panel)
{
  g_return_if_fail (CC_IS_SHELL (self));
  g_return_if_fail (panel == NULL || CC_IS_PANEL (panel));

  if (panel != self->active_panel)
    {
      /* remove the old panel */
      g_clear_object (&self->active_panel);

      /* set the new panel */
      if (panel)
        self->active_panel = g_object_ref (panel);

      g_object_notify (G_OBJECT (self), "active-panel");
    }
}

static void
switch_to_previous_panel (CcWindow *self)
{
  g_autofree gchar *previous_panel_id = NULL;

  CC_ENTRY;

  if (g_queue_get_length (self->previous_panels) == 0)
    CC_RETURN ();

  previous_panel_id = g_queue_pop_head (self->previous_panels);

  g_debug ("Going to previous panel (%s)", previous_panel_id);

  set_active_panel_from_id (self, previous_panel_id, NULL, FALSE, FALSE, NULL);

  CC_EXIT;
}

/* Callbacks */

static void
on_split_view_collapsed_changed_cb (CcWindow *self)
{
  GtkSelectionMode selection_mode;
  gboolean collapsed;

  g_assert (CC_IS_WINDOW (self));

  collapsed = adw_navigation_split_view_get_collapsed (self->split_view);

  selection_mode = collapsed ? GTK_SELECTION_NONE : GTK_SELECTION_SINGLE;
  cc_panel_list_set_selection_mode (self->panel_list, selection_mode);

  g_object_notify (G_OBJECT (self), "collapsed");
}

static void
show_panel_cb (CcWindow    *self,
               const gchar *panel_id,
               const gchar *parent_id)
{
  GVariant *parameters;
  GVariantBuilder builder;

  if (!panel_id)
    return;

  if (!parent_id)
    {
      set_active_panel_from_id (self, panel_id, NULL, TRUE, FALSE, NULL);

      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));
  g_variant_builder_add (&builder, "v", g_variant_new_string (panel_id));
  parameters = g_variant_builder_end (&builder);

  set_active_panel_from_id (self, parent_id, parameters, TRUE, FALSE, NULL);
}

static void
search_entry_activate_cb (CcWindow *self)
{
  gboolean changed;

  if (cc_panel_list_get_view (self->panel_list) != CC_PANEL_LIST_SEARCH)
    return;

  changed = cc_panel_list_activate (self->panel_list);

  gtk_search_bar_set_search_mode (self->search_bar, !changed);
}

static gboolean
go_back_shortcut_cb (GtkWidget *widget,
                     GVariant  *args,
                     gpointer   user_data)
{
  g_debug ("Going to previous panel");
  switch_to_previous_panel (CC_WINDOW (widget));

  return GDK_EVENT_STOP;
}

static gboolean
search_shortcut_cb (GtkWidget *widget,
                    GVariant  *args,
                    gpointer   user_data)
{
  CcPanelListView view;
  CcWindow *self;
  gboolean search;

  self = CC_WINDOW (widget);
  view = cc_panel_list_get_view (self->panel_list);

  /* The search only happens when we're in the MAIN view */
  if (view != CC_PANEL_LIST_MAIN && view != CC_PANEL_LIST_SEARCH)
    return GDK_EVENT_PROPAGATE;

  search = !gtk_search_bar_get_search_mode (self->search_bar);
  gtk_search_bar_set_search_mode (self->search_bar, search);
  if (search)
    gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));

  return GDK_EVENT_STOP;
}

static void
on_development_warning_dialog_responded_cb (CcWindow *self)
{
  g_debug ("Disabling development build warning dialog");
  g_settings_set_boolean (self->settings, "show-development-warning", FALSE);

  gtk_window_close (GTK_WINDOW (self->development_warning_dialog));
}

/* CcShell implementation */
static gboolean
cc_window_set_active_panel_from_id (CcShell      *shell,
                                    const gchar  *start_id,
                                    GVariant     *parameters,
                                    GError      **error)
{
  CcWindow *self = CC_WINDOW (shell);

  g_return_val_if_fail (self != NULL, FALSE);

  cc_panel_list_center_activated_row (self->panel_list, TRUE);
  return set_active_panel_from_id (self, start_id, parameters, TRUE, TRUE, error);
}

static GtkWidget *
cc_window_get_toplevel (CcShell *self)
{
  return GTK_WIDGET (self);
}

static void
cc_shell_iface_init (CcShellInterface *iface)
{
  iface->set_active_panel_from_id = cc_window_set_active_panel_from_id;
  iface->get_toplevel = cc_window_get_toplevel;
}

/* GtkWidget overrides */
static void
cc_window_map (GtkWidget *widget)
{
  CcWindow *self = (CcWindow *) widget;

  GTK_WIDGET_CLASS (cc_window_parent_class)->map (widget);

  /* Show a warning for Flatpak builds */
  if (in_flatpak_sandbox () && g_settings_get_boolean (self->settings, "show-development-warning"))
    gtk_window_present (GTK_WINDOW (self->development_warning_dialog));
}

static void
cc_window_unmap (GtkWidget *widget)
{
  CcWindow *self = CC_WINDOW (widget);
  gboolean maximized;
  gint height;
  gint width;

  maximized = gtk_window_is_maximized (GTK_WINDOW (self));
  gtk_window_get_default_size (GTK_WINDOW (self), &width, &height);

  g_settings_set (self->settings,
                  "window-state",
                  "(iib)",
                  width,
                  height,
                  maximized);

  GTK_WIDGET_CLASS (cc_window_parent_class)->unmap (widget);
}

/* GObject Implementation */
static void
cc_window_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  CcWindow *self = CC_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      g_value_set_object (value, self->active_panel);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->store);
      break;

    case PROP_COLLAPSED:
      g_value_set_boolean (value, adw_navigation_split_view_get_collapsed (self->split_view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_window_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  CcWindow *self = CC_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      set_active_panel (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      g_assert (self->store == NULL);
      self->store = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
maybe_load_last_panel (CcWindow *self)
{
  g_autofree char *id = NULL;

  id = g_settings_get_string (self->settings, "last-panel");
  if (cc_panel_list_get_current_panel (self->panel_list))
    return;

  /* select the last used panel, if any, or the first visible panel */
  if (id != NULL && cc_shell_model_has_panel (self->store, id))
    {
      cc_panel_list_center_activated_row (self->panel_list, TRUE);
      cc_panel_list_set_active_panel (self->panel_list, id);
    }
  else
    cc_panel_list_activate (self->panel_list);
}

static void
cc_window_constructed (GObject *object)
{
  CcWindow *self = CC_WINDOW (object);

  load_window_state (self);

  /* Add the panels */
  setup_model (self);

  /* After everything is loaded, select the last used panel, if any,
   * or the first visible panel. We do that in an idle handler so we
   * have a chance to skip it when another panel has been explicitly
   * activated from commandline parameter or from DBus method */
  g_idle_add_once ((GSourceOnceFunc) maybe_load_last_panel, self);

  G_OBJECT_CLASS (cc_window_parent_class)->constructed (object);
}

static void
cc_window_dispose (GObject *object)
{
  CcWindow *self = CC_WINDOW (object);

  g_clear_pointer (&self->current_panel_id, g_free);
  g_clear_object (&self->store);
  g_clear_object (&self->active_panel);

  G_OBJECT_CLASS (cc_window_parent_class)->dispose (object);
}

static void
cc_window_finalize (GObject *object)
{
  CcWindow *self = CC_WINDOW (object);

  if (self->previous_panels)
    {
      g_queue_free_full (self->previous_panels, g_free);
      self->previous_panels = NULL;
    }

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (cc_window_parent_class)->finalize (object);
}

static gboolean
search_entry_key_pressed_cb (CcWindow              *self,
                             guint                  keyval,
                             guint                  keycode,
                             GdkModifierType        state,
                             GtkEventControllerKey *key_controller)
{
  GtkWidget *toplevel;

  /* When pressing Arrow Down on the entry we move focus to match results list */
  if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
    {
      toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

      if (!toplevel)
        return FALSE;

      return gtk_widget_child_focus (toplevel, GTK_DIR_TAB_FORWARD);
    }

  return FALSE;
}

static void
cc_window_class_init (CcWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_window_get_property;
  object_class->set_property = cc_window_set_property;
  object_class->constructed = cc_window_constructed;
  object_class->dispose = cc_window_dispose;
  object_class->finalize = cc_window_finalize;

  widget_class->map = cc_window_map;
  widget_class->unmap = cc_window_unmap;

  g_object_class_override_property (object_class, PROP_ACTIVE_PANEL, "active-panel");

  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        "Model",
                                                        "The CcShellModel of this application",
                                                        CC_TYPE_SHELL_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   PROP_COLLAPSED,
                                   g_param_spec_boolean ("collapsed",
                                                         "Collapsed",
                                                         "Whether the window is collapsed",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Settings/gtk/cc-window.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWindow, development_warning_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, panel_list);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, search_bar);
  gtk_widget_class_bind_template_child (widget_class, CcWindow, search_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_split_view_collapsed_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_development_warning_dialog_responded_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_panel_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_key_pressed_cb);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Left, GDK_ALT_MASK, go_back_shortcut_cb, NULL);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_f, GDK_CONTROL_MASK, search_shortcut_cb, NULL);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_F, GDK_CONTROL_MASK, search_shortcut_cb, NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_q, GDK_CONTROL_MASK, "window.close", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Q, GDK_CONTROL_MASK, "window.close", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_w, GDK_CONTROL_MASK, "window.close", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_W, GDK_CONTROL_MASK, "window.close", NULL);

  g_type_ensure (CC_TYPE_PANEL_LIST);
}

static void
cc_window_init (CcWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->settings = g_settings_new ("org.gnome.Settings");
  self->previous_panels = g_queue_new ();
  self->previous_list_view = cc_panel_list_get_view (self->panel_list);

  /* Add a custom CSS class on development builds */
  if (in_flatpak_sandbox ())
    gtk_widget_add_css_class (GTK_WIDGET (self), "devel");

  gtk_search_bar_set_key_capture_widget (self->search_bar, GTK_WIDGET (self));
}

CcWindow *
cc_window_new (GtkApplication *application,
               CcShellModel   *model)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return g_object_new (CC_TYPE_WINDOW,
                       "application", application,
                       "resizable", TRUE,
                       "title", _("Settings"),
                       "icon-name", DEFAULT_WINDOW_ICON_NAME,
                       "show-menubar", FALSE,
                       "model", model,
                       NULL);
}

void
cc_window_set_search_item (CcWindow   *center,
                           const char *search)
{
  gtk_search_bar_set_search_mode (center->search_bar, TRUE);
  gtk_editable_set_text (GTK_EDITABLE (center->search_entry), search);
  gtk_editable_set_position (GTK_EDITABLE (center->search_entry), -1);
}
