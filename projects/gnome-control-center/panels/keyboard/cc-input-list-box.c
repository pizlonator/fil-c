/* cc-input-list-box.c
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2020 System76, Inc.
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
 * Author: Sergey Udaltsov   <svu@gnome.org>
 *         Ian Douglas Scott <idscott@system76.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#include "cc-input-list-box.h"
#include "cc-input-chooser.h"
#include "cc-input-row.h"
#include "cc-input-source-ibus.h"
#include "cc-input-source-xkb.h"

#ifdef HAVE_IBUS
#include <ibus.h>
#endif

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"
#define KEY_INPUT_SOURCES        "sources"
#define KEY_MRU_SOURCES          "mru-sources"

struct _CcInputListBox {
  AdwBin          parent_instance;

  GtkListBoxRow   *add_input_row;
  GtkListBox      *listbox;
  AdwActionRow    *no_inputs_row;

  GCancellable    *cancellable;

  gboolean     login;
  gboolean     login_auto_apply;
  GPermission *permission;
  GDBusProxy  *localed;

  GSettings *input_settings;
  GnomeXkbInfo *xkb_info;
#ifdef HAVE_IBUS
  IBusBus *ibus;
  GHashTable *ibus_engines;
#endif
};

G_DEFINE_TYPE (CcInputListBox, cc_input_list_box, ADW_TYPE_BIN)
 
typedef struct
{
  CcInputListBox *panel;
  CcInputRow    *source;
  CcInputRow    *dest;
} RowData;

static RowData *
row_data_new (CcInputListBox *panel, CcInputRow *source, CcInputRow *dest)
{
  RowData *data = g_malloc0 (sizeof (RowData));
  data->panel = panel;
  data->source = g_object_ref (source);
  if (dest != NULL)
    data->dest = g_object_ref (dest);
  return data;
}

static void
row_data_free (RowData *data)
{
  g_clear_object (&data->source);
  g_clear_object (&data->dest);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (RowData, row_data_free)

static void show_input_chooser (CcInputListBox *self);

#ifdef HAVE_IBUS
static void
update_ibus_active_sources (CcInputListBox *self)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child;
       child = gtk_widget_get_next_sibling (child)) {
    CcInputRow *row;
    CcInputSourceIBus *source;
    IBusEngineDesc *engine_desc;

    if (!CC_IS_INPUT_ROW (child))
      continue;
    row = CC_INPUT_ROW (child);

    if (!CC_IS_INPUT_SOURCE_IBUS (cc_input_row_get_source (row)))
      continue;
    source = CC_INPUT_SOURCE_IBUS (cc_input_row_get_source (row));

    engine_desc = g_hash_table_lookup (self->ibus_engines, cc_input_source_ibus_get_engine_name (source));
    if (engine_desc != NULL)
      cc_input_source_ibus_set_engine_desc (source, engine_desc);
  }
}

static void
fetch_ibus_engines_result (GObject       *object,
                           GAsyncResult  *result,
                           CcInputListBox *self)
{
  g_autoptr(GList) list = NULL;
  GList *l;
  g_autoptr(GError) error = NULL;

  list = ibus_bus_list_engines_async_finish (IBUS_BUS (object), result, &error);
  if (!list && error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	g_warning ("Couldn't finish IBus request: %s", error->message);
      return;
  }

  /* Maps engine ids to engine description objects */
  self->ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  for (l = list; l; l = l->next) {
    IBusEngineDesc *engine = l->data;
    const gchar *engine_id = ibus_engine_desc_get_name (engine);

    if (g_str_has_prefix (engine_id, "xkb:"))
      g_object_unref (engine);
    else
      g_hash_table_replace (self->ibus_engines, (gpointer)engine_id, engine);
  }

  update_ibus_active_sources (self);
}

static void
fetch_ibus_engines (CcInputListBox *self)
{
  ibus_bus_list_engines_async (self->ibus,
			       -1,
			       self->cancellable,
			       (GAsyncReadyCallback)fetch_ibus_engines_result,
			       self);

  /* We've got everything we needed, don't want to be called again. */
  g_signal_handlers_disconnect_by_func (self->ibus, fetch_ibus_engines, self);
}

static void
maybe_start_ibus (void)
{
  /* IBus doesn't export API in the session bus. The only thing
   * we have there is a well known name which we can use as a
   * sure-fire way to activate it.
   */
  g_bus_unwatch_name (g_bus_watch_name (G_BUS_TYPE_SESSION,
					IBUS_SERVICE_IBUS,
					G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
					NULL,
					NULL,
					NULL,
					NULL));
}

#endif

static gboolean
keynav_failed_cb (CcInputListBox   *self,
                  GtkDirectionType  direction,
                  GtkWidget        *list)
{
  GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

  if (!toplevel)
    return FALSE;

  if (direction != GTK_DIR_UP && direction != GTK_DIR_DOWN)
    return FALSE;

  return gtk_widget_child_focus (toplevel, direction == GTK_DIR_UP ?
                                 GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
}

static void
row_settings_cb (CcInputListBox *self,
                 CcInputRow    *row)
{
  CcInputSourceIBus *source;
  g_autoptr(GdkAppLaunchContext) ctx = NULL;
  GDesktopAppInfo *app_info;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (CC_IS_INPUT_SOURCE_IBUS (cc_input_row_get_source (row)));
  source = CC_INPUT_SOURCE_IBUS (cc_input_row_get_source (row));

  app_info = cc_input_source_ibus_get_app_info (source);
  if  (app_info == NULL)
    return;

  ctx = gdk_display_get_app_launch_context (gdk_display_get_default ());
  gdk_app_launch_context_set_timestamp (ctx, GDK_CURRENT_TIME);

  g_app_launch_context_setenv (G_APP_LAUNCH_CONTEXT (ctx),
			       "IBUS_ENGINE_NAME", cc_input_source_ibus_get_engine_name (source));

  if (!g_app_info_launch (G_APP_INFO (app_info), NULL, G_APP_LAUNCH_CONTEXT (ctx), &error))
    g_warning ("Failed to launch input source setup: %s", error->message);
}

static void
row_layout_cb (CcInputListBox *self,
               CcInputRow    *row)
{
  CcInputSource *source;
  const gchar *layout, *layout_variant;
  g_autofree gchar *commandline = NULL;

  source = cc_input_row_get_source (row);

  layout = cc_input_source_get_layout (source);
  layout_variant = cc_input_source_get_layout_variant (source);

  if (layout_variant && layout_variant[0])
    commandline = g_strdup_printf (KEYBOARD_PREVIEWER_EXEC " \"%s+%s\"",
				   layout, layout_variant);
  else
    commandline = g_strdup_printf (KEYBOARD_PREVIEWER_EXEC " %s",
				   layout);

  g_debug ("Launching keyboard previewer with command line: '%s'\n", commandline);
  g_spawn_command_line_async (commandline, NULL);
}

static void move_input (CcInputListBox *self, CcInputRow *source, CcInputRow *dest);

static void
row_moved_cb (CcInputListBox *self,
              CcInputRow    *dest_row,
              CcInputRow    *row)
{
  move_input (self, row, dest_row);
}

static void remove_input (CcInputListBox *self, CcInputRow *row);

static void
row_removed_cb (CcInputListBox *self,
                CcInputRow    *row)
{
  remove_input (self, row);
}

static void
update_input_rows (CcInputListBox *self)
{
  GtkWidget *child;
  guint n_input_rows = 0;

  child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
  while ((child = gtk_widget_get_next_sibling (child)) != NULL)
    if (CC_IS_INPUT_ROW (child))
      n_input_rows++;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child;
       child = gtk_widget_get_next_sibling (child)) {
    CcInputRow *row;
    gint row_idx;

    if (!CC_IS_INPUT_ROW (child))
      continue;
    row = CC_INPUT_ROW (child);
    row_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row));

    cc_input_row_set_removable (row, n_input_rows > 1);
    cc_input_row_set_draggable (row, n_input_rows > 1);

    gtk_widget_action_set_enabled (GTK_WIDGET (row), "row.move-up", row_idx != 1);
    gtk_widget_action_set_enabled (GTK_WIDGET (row), "row.move-down", GTK_LIST_BOX_ROW (gtk_widget_get_next_sibling (child)) != self->add_input_row);

  }
}

static void
add_input_row (CcInputListBox *self, CcInputSource *source)
{
  CcInputRow *row;

  gtk_widget_set_visible (GTK_WIDGET (self->no_inputs_row), FALSE);

  row = cc_input_row_new (source);
  g_signal_connect_object (row, "show-settings", G_CALLBACK (row_settings_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (row, "show-layout", G_CALLBACK (row_layout_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (row, "move-row", G_CALLBACK (row_moved_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (row, "remove-row", G_CALLBACK (row_removed_cb), self, G_CONNECT_SWAPPED);
  gtk_list_box_insert (self->listbox, GTK_WIDGET (row), gtk_list_box_row_get_index (self->add_input_row));
  update_input_rows (self);
}

static void
add_input_sources (CcInputListBox *self,
                   GVariant      *sources)
{
  GVariantIter iter;
  const gchar *type, *id;

  if (g_variant_n_children (sources) < 1) {
    gtk_widget_set_visible (GTK_WIDGET (self->no_inputs_row), TRUE);
    return;
  }

  g_variant_iter_init (&iter, sources);
  while (g_variant_iter_next (&iter, "(&s&s)", &type, &id)) {
    g_autoptr(CcInputSource) source = NULL;

    if (g_str_equal (type, "xkb")) {
      source = CC_INPUT_SOURCE (cc_input_source_xkb_new_from_id (self->xkb_info, id));
    } else if (g_str_equal (type, "ibus")) {
      source = CC_INPUT_SOURCE (cc_input_source_ibus_new (id));
#ifdef HAVE_IBUS
      if (self->ibus_engines) {
	IBusEngineDesc *engine_desc = g_hash_table_lookup (self->ibus_engines, id);
	if (engine_desc != NULL)
	  cc_input_source_ibus_set_engine_desc (CC_INPUT_SOURCE_IBUS (source), engine_desc);
	}
#endif
    } else {
      g_warning ("Unhandled input source type '%s'", type);
      continue;
    }

    add_input_row (self, source);
  }
}

static void
add_input_sources_from_settings (CcInputListBox *self)
{
  g_autoptr(GVariant) sources = NULL;
  sources = g_settings_get_value (self->input_settings, "sources");
  add_input_sources (self, sources);
}

static void
clear_input_sources (CcInputListBox *self)
{
  GtkWidget *child;

  child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling (child);

    if (CC_IS_INPUT_ROW (child))
      gtk_list_box_remove (self->listbox, GTK_WIDGET (child));

    child = next;
  }
}

static CcInputRow *
get_row_by_source (CcInputListBox *self, CcInputSource *source)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child;
       child = gtk_widget_get_next_sibling (child)) {
    CcInputRow *row;

    if (!CC_IS_INPUT_ROW (child))
      continue;
    row = CC_INPUT_ROW (child);

    if (cc_input_source_matches (source, cc_input_row_get_source (row)))
      return row;
  }

  return NULL;
}

static void
input_sources_changed (CcInputListBox *self,
                       const gchar   *key)
{
  CcInputRow *selected;
  g_autoptr(CcInputSource) source = NULL;

  selected = CC_INPUT_ROW (gtk_list_box_get_selected_row (self->listbox));
  if (selected)
    source = g_object_ref (cc_input_row_get_source (selected));
  clear_input_sources (self);
  add_input_sources_from_settings (self);
  if (source != NULL) {
    CcInputRow *row = get_row_by_source (self, source);
    if (row != NULL)
      gtk_list_box_select_row (self->listbox, GTK_LIST_BOX_ROW (row));
  }
}

static void
set_input_settings (CcInputListBox *self)
{
  GVariantBuilder builder;
  GtkWidget *child;
  GVariant *value;
  GVariant *previous_value = g_settings_get_value (self->input_settings, KEY_INPUT_SOURCES);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child;
       child = gtk_widget_get_next_sibling (child)) {
    CcInputRow *row;
    CcInputSource *source;

    if (!CC_IS_INPUT_ROW (child))
      continue;
    row = CC_INPUT_ROW (child);
    source = cc_input_row_get_source (row);

    if (CC_IS_INPUT_SOURCE_XKB (source)) {
      g_autofree gchar *id = cc_input_source_xkb_get_id (CC_INPUT_SOURCE_XKB (source));
      g_variant_builder_add (&builder, "(ss)", "xkb", id);
    } else if (CC_IS_INPUT_SOURCE_IBUS (source)) {
      g_variant_builder_add (&builder, "(ss)", "ibus",
			     cc_input_source_ibus_get_engine_name (CC_INPUT_SOURCE_IBUS (source)));
    }
  }

  value = g_variant_ref_sink (g_variant_builder_end (&builder));
  g_settings_set_value (self->input_settings, KEY_INPUT_SOURCES, value);

  /* We need to make sure it's always possible to compute the current input
   * source from the settings, e.g. so distro installers can distinguish between
   * configured input sources vs. current input source. Writing the sources
   * setting alone is insufficient. If the mru-sources setting has never been
   * written, then the user has never changed input sources, and we can set
   * mru-sources to the previous value of the sources setting to indicate that
   * the first previously-configured input source is the current input source.
   * If mru-sources has been written, then the user has changed input sources
   * and we don't need to do anything extra.
   */
  if (g_settings_get_user_value (self->input_settings, KEY_MRU_SOURCES) == NULL)
    g_settings_set_value (self->input_settings, KEY_MRU_SOURCES, previous_value);

  g_variant_unref (value);
  g_variant_unref (previous_value);
}

static void set_localed_input (CcInputListBox *self);

static void
update_input (CcInputListBox *self)
{
  if (self->login) {
    set_localed_input (self);
  } else {
    set_input_settings (self);
    if (self->login_auto_apply)
      set_localed_input (self);
  }
}

static void
on_chooser_response_cb (CcInputListBox *self,
                        CcInputSource  *source)
{
  if (source) {
    if (source != NULL && get_row_by_source (self, source) == NULL) {
      add_input_row (self, source);
      update_input (self);
    }
  }
}

static void
show_input_chooser (CcInputListBox *self)
{
  CcInputChooser *chooser;

  chooser = cc_input_chooser_new (self->login,
				  self->xkb_info,
#ifdef HAVE_IBUS
				  self->ibus_engines
#else
				  NULL
#endif
				  );
  gtk_window_set_transient_for (GTK_WINDOW (chooser),
                                GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
  g_signal_connect_swapped (chooser, "source-selected", G_CALLBACK (on_chooser_response_cb), self);
  gtk_window_present (GTK_WINDOW (chooser));
}

// Duplicated from cc-region-panel.c
static gboolean
permission_acquired (GPermission *permission, GAsyncResult *res, const gchar *action)
{
  g_autoptr(GError) error = NULL;

  if (!g_permission_acquire_finish (permission, res, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to acquire permission to %s: %s\n", error->message, action);
    return FALSE;
  }

  return TRUE;
}

static void
add_input_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  CcInputListBox *self = user_data;
  if (permission_acquired (G_PERMISSION (source), res, "add input"))
    show_input_chooser (self);
}

static void
add_input (CcInputListBox *self)
{
  if (!self->login) {
    show_input_chooser (self);
  } else if (g_permission_get_allowed (self->permission)) {
    show_input_chooser (self);
  } else if (g_permission_get_can_acquire (self->permission)) {
    g_permission_acquire_async (self->permission,
				self->cancellable,
				add_input_permission_cb,
				self);
  }
}

static GtkWidget *
find_sibling (GtkWidget *child)
{
  GtkWidget *sibling;

  for (sibling = gtk_widget_get_next_sibling (child);
       sibling;
       sibling = gtk_widget_get_next_sibling (child)) {
    if (gtk_widget_get_visible (sibling) && gtk_widget_get_child_visible (sibling))
      return sibling;
  }

  for (sibling = gtk_widget_get_prev_sibling (child);
       sibling;
       sibling = gtk_widget_get_prev_sibling (child)) {
    if (gtk_widget_get_visible (sibling) && gtk_widget_get_child_visible (sibling))
      return sibling;
  }

  return NULL;
}

static void
do_remove_input (CcInputListBox *self, CcInputRow *row)
{
        GtkWidget *sibling;

        sibling = find_sibling (GTK_WIDGET (row));
        gtk_list_box_remove (self->listbox, GTK_WIDGET (row));
        gtk_list_box_select_row (self->listbox, GTK_LIST_BOX_ROW (sibling));

        update_input (self);
        update_input_rows (self);
}

static void
remove_input_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  RowData *data = user_data;
  if (permission_acquired (G_PERMISSION (source), res, "remove input"))
    do_remove_input (data->panel, data->source);
}

static void
remove_input (CcInputListBox *self, CcInputRow *row)
{
  if (!self->login) {
    do_remove_input (self, row);
  } else if (g_permission_get_allowed (self->permission)) {
    do_remove_input (self, row);
  } else if (g_permission_get_can_acquire (self->permission)) {
    g_permission_acquire_async (self->permission,
				self->cancellable,
				remove_input_permission_cb,
				row_data_new (self, row, NULL));
  }
}

static void
do_move_input (CcInputListBox *self, CcInputRow *source, CcInputRow *dest)
{
  gint dest_index;

  dest_index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (dest));

  g_object_ref (source);
  gtk_list_box_remove (self->listbox, GTK_WIDGET (source));
  gtk_list_box_insert (self->listbox, GTK_WIDGET (source), dest_index);
  g_object_unref (source);

  update_input (self);
}

static void
move_input_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
  RowData *data = user_data;
  if (permission_acquired (G_PERMISSION (source), res, "move input"))
    do_move_input (data->panel, data->source, data->dest);
}

static void
move_input (CcInputListBox *self,
            CcInputRow    *source,
            CcInputRow    *dest)
{
  if (!self->login) {
    do_move_input (self, source, dest);
  } else if (g_permission_get_allowed (self->permission)) {
    do_move_input (self, source, dest);
  } else if (g_permission_get_can_acquire (self->permission)) {
    g_permission_acquire_async (self->permission,
				self->cancellable,
				move_input_permission_cb,
				row_data_new (self, source, dest));
  }
}

static void
input_row_activated_cb (CcInputListBox *self, GtkListBoxRow *row)
{
  if (row == self->add_input_row) {
    add_input (self);
  }
}

static void
add_input_sources_from_localed (CcInputListBox *self)
{
  g_autoptr(GVariant) layout_property = NULL;
  g_autoptr(GVariant) variant_property = NULL;
  const gchar *s;
  g_auto(GStrv) layouts = NULL;
  g_auto(GStrv) variants = NULL;
  gint i, n;

  if (!self->localed)
    return;

  layout_property = g_dbus_proxy_get_cached_property (self->localed, "X11Layout");
  if (layout_property) {
    s = g_variant_get_string (layout_property, NULL);
    layouts = g_strsplit (s, ",", -1);
  }

  variant_property = g_dbus_proxy_get_cached_property (self->localed, "X11Variant");
  if (variant_property) {
    s = g_variant_get_string (variant_property, NULL);
    if (s && *s)
      variants = g_strsplit (s, ",", -1);
  }

  if (variants && variants[0])
    n = MIN (g_strv_length (layouts), g_strv_length (variants));
  else if (layouts && layouts[0])
    n = g_strv_length (layouts);
  else
    n = 0;

  for (i = 0; i < n && layouts[i][0]; i++) {
    const char *variant = variants ? variants[i] : NULL;
    g_autoptr(CcInputSourceXkb) source = cc_input_source_xkb_new (self->xkb_info, layouts[i], variant);
    add_input_row (self, CC_INPUT_SOURCE (source));
  }
  gtk_widget_set_visible (GTK_WIDGET (self->no_inputs_row), n == 0);
}

static void
set_localed_input (CcInputListBox *self)
{
  g_autoptr(GString) layouts = NULL;
  g_autoptr(GString) variants = NULL;
  GtkWidget *child;

  layouts = g_string_new ("");
  variants = g_string_new ("");

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->listbox));
       child;
       child = gtk_widget_get_next_sibling (child)) {
    CcInputRow *row;
    CcInputSourceXkb *source;
    g_autofree gchar *id = NULL;
    const gchar *l, *v;

    if (!CC_IS_INPUT_ROW (child))
      continue;
    row = CC_INPUT_ROW (child);

    if (!CC_IS_INPUT_SOURCE_XKB (cc_input_row_get_source (row)))
      continue;
    source = CC_INPUT_SOURCE_XKB (cc_input_row_get_source (row));

    id = cc_input_source_xkb_get_id (source);
    if (gnome_xkb_info_get_layout_info (self->xkb_info, id, NULL, NULL, &l, &v)) {
      if (layouts->str[0]) {
        g_string_append_c (layouts, ',');
        g_string_append_c (variants, ',');
      }
      g_string_append (layouts, l);
      g_string_append (variants, v);
    }
  }

  g_dbus_proxy_call (self->localed,
                     "SetX11Keyboard",
                     g_variant_new ("(ssssbb)", layouts->str, "", variants->str, "", TRUE, TRUE),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}

static void
cc_input_list_box_finalize (GObject *object)
{
  CcInputListBox *self = CC_INPUT_LIST_BOX (object);

  g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->input_settings);
  g_clear_object (&self->xkb_info);
#ifdef HAVE_IBUS
  g_clear_object (&self->ibus);
  g_clear_pointer (&self->ibus_engines, g_hash_table_destroy);
#endif

  G_OBJECT_CLASS (cc_input_list_box_parent_class)->finalize (object);
}

static void
cc_input_list_box_class_init (CcInputListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_input_list_box_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-input-list-box.ui");

  gtk_widget_class_bind_template_child (widget_class, CcInputListBox, add_input_row);
  gtk_widget_class_bind_template_child (widget_class, CcInputListBox, listbox);
  gtk_widget_class_bind_template_child (widget_class, CcInputListBox, no_inputs_row);

  gtk_widget_class_bind_template_callback (widget_class, input_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, keynav_failed_cb);
}

static void
cc_input_list_box_init (CcInputListBox *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->login = FALSE;
  self->login_auto_apply = FALSE;
  self->localed = NULL;
  self->permission = NULL;

  self->cancellable = g_cancellable_new();

  self->input_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);

  self->xkb_info = gnome_xkb_info_new ();

#ifdef HAVE_IBUS
  ibus_init ();
  if (!self->ibus) {
    self->ibus = ibus_bus_new_async ();
    if (ibus_bus_is_connected (self->ibus))
      fetch_ibus_engines (self);
    else
      g_signal_connect_object (self->ibus, "connected",
                               G_CALLBACK (fetch_ibus_engines), self,
                               G_CONNECT_SWAPPED);
  }
  maybe_start_ibus ();
#endif

  g_signal_connect_object (self->input_settings, "changed::" KEY_INPUT_SOURCES,
                           G_CALLBACK (input_sources_changed), self, G_CONNECT_SWAPPED);

  add_input_sources_from_settings (self);
}

void
cc_input_list_box_set_login (CcInputListBox *self, gboolean login)
{
  self->login = login;
  clear_input_sources (self);
  if (login)
    add_input_sources_from_localed (self);
  else
    add_input_sources_from_settings (self);
}

void
cc_input_list_box_set_login_auto_apply (CcInputListBox *self, gboolean login_auto_apply)
{
  self->login_auto_apply = login_auto_apply;
}

void
cc_input_list_box_set_localed (CcInputListBox *self, GDBusProxy *localed)
{
  self->localed = localed;
}

void
cc_input_list_box_set_permission (CcInputListBox *self, GPermission *permission)
{
  self->permission = permission;
}
