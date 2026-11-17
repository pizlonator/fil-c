/*
 * Copyright © 2011 Red Hat, Inc.
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
 * Authors: Peter Hutterer <peter.hutterer@redhat.com>
 *          Bastien Nocera <hadess@hadess.net>
 *
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "shell/cc-application.h"
#include "shell/cc-log.h"
#include "cc-wacom-panel.h"
#include "cc-wacom-page.h"
#include "cc-wacom-ekr-page.h"
#include "cc-wacom-stylus-page.h"
#include "cc-wacom-resources.h"
#include "cc-drawing-area.h"
#include "cc-tablet-tool-map.h"
#include "gsd-device-manager.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#define POLL_MS 300

struct _CcWacomPanel
{
	CcPanel                 parent_instance;

	GtkWidget              *test_popover;
	GtkWidget              *test_draw_area;
	GtkWidget              *test_button;
	GtkWidget              *scrollable;
	GtkWidget              *tablets;
	GtkWidget              *styli;
	GtkWidget              *initial_state_stack;
	GtkWidget              *panel_view;
	GtkWidget              *panel_empty_state;
	GHashTable             *devices; /* key=GsdDevice, value=CcWacomDevice */
	GHashTable             *pages; /* key=CcWacomDevice, value=GtkWidget */
	GHashTable             *stylus_pages; /* key=CcWacomTool, value=CcWacomStylusPage */
	guint                   mock_stylus_id;

	CcTabletToolMap        *tablet_tool_map;

	GtkAdjustment          *vadjustment;
	GtkGesture             *stylus_gesture;

	GtkWidget              *highlighted_widget;
	CcWacomStylusPage      *highlighted_stylus_page;
	guint                   highlight_timeout_id;

	/* DBus */
	GDBusProxy             *proxy;
	GDBusProxy             *input_mapping_proxy;
};

CC_PANEL_REGISTER (CcWacomPanel, cc_wacom_panel)

typedef struct {
	const char *name;
	CcWacomDevice *stylus;
	CcWacomDevice *pad;
} Tablet;

enum {
	WACOM_PAGE = -1,
	PLUG_IN_PAGE = 0,
};

enum {
	PROP_0,
	PROP_PARAMETERS
};

/* Static init function */
static void
update_visibility (GsdDeviceManager *manager,
		   GsdDevice        *device,
		   gpointer          user_data)
{
	CcApplication *application;
	g_autoptr(GList) devices = NULL;
	guint i;

	devices = gsd_device_manager_list_devices (manager, GSD_DEVICE_TYPE_TABLET);
	i = g_list_length (devices);

	/* Set the new visibility */
	application = CC_APPLICATION (g_application_get_default ());
	cc_shell_model_set_panel_visibility (cc_application_get_model (application),
					     "wacom",
					     i > 0 ? CC_PANEL_VISIBLE : CC_PANEL_VISIBLE_IN_SEARCH);

	g_debug ("Wacom panel visible: %s", i > 0 ? "yes" : "no");
}

void
cc_wacom_panel_static_init_func (void)
{
	GsdDeviceManager *manager;

	manager = gsd_device_manager_get ();
	g_signal_connect (G_OBJECT (manager), "device-added",
			  G_CALLBACK (update_visibility), NULL);
	g_signal_connect (G_OBJECT (manager), "device-removed",
			  G_CALLBACK (update_visibility), NULL);
	update_visibility (manager, NULL, NULL);
}

static CcWacomDevice *
lookup_wacom_device (CcWacomPanel *self,
		     const gchar  *name)
{
	GHashTableIter iter;
	CcWacomDevice *wacom_device;

	g_hash_table_iter_init (&iter, self->devices);
	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &wacom_device)) {
		if (g_strcmp0 (cc_wacom_device_get_name (wacom_device), name) == 0)
			return wacom_device;
	}

	return NULL;
}

static void
highlight_widget (CcWacomPanel *self, GtkWidget *widget)
{
	graphene_point_t p;

	if (self->highlighted_widget == widget)
		return;

	if (!gtk_widget_compute_point (widget,
                                       self->scrollable,
                                       &GRAPHENE_POINT_INIT (0, 0),
                                       &p))
		return;

	gtk_adjustment_set_value (self->vadjustment, p.y);
	self->highlighted_widget = widget;
}

static CcWacomPage *
update_highlighted_device (CcWacomPanel *self, const gchar *device_name)
{
	CcWacomPage *page;
	CcWacomDevice *wacom_device;

	if (device_name == NULL)
		return NULL;

	wacom_device = lookup_wacom_device (self, device_name);
	if (!wacom_device) {
		g_warning ("Failed to find device '%s', supplied in the command line.", device_name);
		return NULL;
	}

	page = g_hash_table_lookup (self->pages, wacom_device);
	highlight_widget (self, GTK_WIDGET (page));

	return page;
}

static void
run_operation_from_params (CcWacomPanel *self, GVariant *parameters)
{
	g_autoptr(GVariant) v = NULL;
	g_autoptr(GVariant) v2 = NULL;
	CcWacomPage *page;
	const gchar *operation = NULL;
	const gchar *device_name = NULL;
	gint n_params;

	n_params = g_variant_n_children (parameters);

	g_variant_get_child (parameters, n_params - 1, "v", &v);
	device_name = g_variant_get_string (v, NULL);

	if (!g_variant_is_of_type (v, G_VARIANT_TYPE_STRING)) {
		g_warning ("Wrong type for the second argument GVariant, expected 's' but got '%s'",
			   g_variant_get_type_string (v));
		return;
	}

	switch (n_params) {
		case 3:
			page = update_highlighted_device (self, device_name);
			if (page == NULL)
				return;

			g_variant_get_child (parameters, 1, "v", &v2);

			if (!g_variant_is_of_type (v2, G_VARIANT_TYPE_STRING)) {
				g_warning ("Wrong type for the operation name argument. A string is expected.");
				break;
			}

			operation = g_variant_get_string (v2, NULL);
			if (g_strcmp0 (operation, "run-calibration") == 0) {
				if (cc_wacom_page_can_calibrate (page))
					cc_wacom_page_calibrate (page);
				else
					g_warning ("The device %s cannot be calibrated.", device_name);
			} else {
				g_warning ("Ignoring unrecognized operation '%s'", operation);
			}
		case 2:
			update_highlighted_device (self, device_name);
			break;
		case 1:
			g_assert_not_reached ();
		default:
			g_warning ("Unexpected number of parameters found: %d. Request ignored.", n_params);
	}
}

/* Boilerplate code goes below */

static void
cc_wacom_panel_get_property (GObject    *object,
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
cc_wacom_panel_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	CcWacomPanel *self;
	self = CC_WACOM_PANEL (object);

	switch (property_id)
	{
		case PROP_PARAMETERS: {
			GVariant *parameters;

			parameters = g_value_get_variant (value);
			if (parameters == NULL || g_variant_n_children (parameters) <= 1)
				return;

			run_operation_from_params (self, parameters);

			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
cc_wacom_panel_dispose (GObject *object)
{
	CcWacomPanel *self = CC_WACOM_PANEL (object);
	CcShell *shell;

	shell = cc_panel_get_shell (CC_PANEL (self));
	if (shell) {
		gtk_widget_remove_controller (GTK_WIDGET (shell),
					      GTK_EVENT_CONTROLLER (self->stylus_gesture));
	}


	g_clear_pointer (&self->devices, g_hash_table_unref);
	g_clear_object (&self->proxy);
	g_clear_object (&self->input_mapping_proxy);
	g_clear_pointer (&self->pages, g_hash_table_unref);
	g_clear_pointer (&self->stylus_pages, g_hash_table_unref);
	g_clear_handle_id (&self->mock_stylus_id, g_source_remove);
	g_clear_handle_id (&self->highlight_timeout_id, g_source_remove);
	g_clear_object (&self->highlighted_stylus_page);

	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->dispose (object);
}

static void
check_remove_stylus_pages (CcWacomPanel *self)
{
	GHashTableIter iter;
	CcWacomDevice *device;
	CcWacomTool *tool;
	CcWacomStylusPage *page;
	GList *tools;
	g_autoptr(GList) total = NULL;

	/* First. Iterate known devices and get the tools */
	g_hash_table_iter_init (&iter, self->devices);
	while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &device)) {
		tools = cc_tablet_tool_map_list_tools (self->tablet_tool_map, device);
		total = g_list_concat (total, tools);
	}

	/* Second. Iterate through stylus pages and remove the ones whose
	 * tool is no longer in the list.
	 */
	g_hash_table_iter_init (&iter, self->stylus_pages);
	while (g_hash_table_iter_next (&iter, (gpointer*) &tool, (gpointer*) &page)) {
		if (g_list_find (total, tool))
			continue;

		if (page == self->highlighted_stylus_page) {
			g_clear_object (&self->highlighted_stylus_page);
			g_clear_handle_id (&self->highlight_timeout_id, g_source_remove);
		}

		gtk_box_remove (GTK_BOX (self->styli), GTK_WIDGET (page));
		g_hash_table_iter_remove (&iter);
	}

}

static gboolean
add_stylus (CcWacomPanel *self,
	    CcWacomTool  *tool)
{
	GtkWidget *page;

	if (g_hash_table_lookup (self->stylus_pages, tool))
		return FALSE;

	page = cc_wacom_stylus_page_new (self, tool);
	gtk_box_append (GTK_BOX (self->styli), page);
	g_hash_table_insert (self->stylus_pages, tool, page);

	return TRUE;
}

static void
update_test_button (CcWacomPanel *self)
{
	if (!self->test_button)
		return;

	if (g_hash_table_size (self->devices) == 0) {
		gtk_popover_popdown (GTK_POPOVER (self->test_popover));
		gtk_widget_set_sensitive (self->test_button, FALSE);
	} else {
		gtk_widget_set_sensitive (self->test_button, TRUE);
	}
}

static void
update_initial_state (CcWacomPanel *self)
{
	gtk_stack_set_visible_child (GTK_STACK (self->initial_state_stack),
				     g_hash_table_size (self->devices) == 0 ?
				     self->panel_empty_state :
				     self->panel_view);
}

static void
on_stylus_timeout (gpointer data)
{
	CcWacomPanel *panel = CC_WACOM_PANEL (data);

	cc_wacom_stylus_page_set_highlight (panel->highlighted_stylus_page, FALSE);
	g_clear_object (&panel->highlighted_stylus_page);
	panel->highlight_timeout_id = 0;
}

static void
update_highlighted_stylus (CcWacomPanel *self,
			   CcWacomTool  *stylus_to_highlight)
{
	GHashTableIter iter;
	CcWacomTool *stylus;
	CcWacomStylusPage *page;

	g_hash_table_iter_init (&iter, self->stylus_pages);
	while (g_hash_table_iter_next (&iter, (gpointer *)&stylus, (gpointer *)&page)) {
		gboolean highlight = stylus == stylus_to_highlight;
		cc_wacom_stylus_page_set_highlight (page, highlight);
		if (highlight) {
			highlight_widget (self, GTK_WIDGET (page));
			g_clear_object (&self->highlighted_stylus_page);
			g_clear_handle_id (&self->highlight_timeout_id, g_source_remove);
			self->highlight_timeout_id = g_timeout_add_once (POLL_MS, on_stylus_timeout, self);
			self->highlighted_stylus_page = g_object_ref (page);
		}
	}

}

static void
update_current_tool (CcWacomPanel  *self,
		     GdkDevice     *device,
		     GdkDeviceTool *tool)
{
	GsdDeviceManager *device_manager;
	CcWacomDevice *wacom_device;
	CcWacomTool *stylus;
	GsdDevice *gsd_device;
	guint64 serial, id;

	if (!tool)
		return;

	/* Work our way to the CcWacomDevice */
	device_manager = gsd_device_manager_get ();
	gsd_device = gsd_device_manager_lookup_gdk_device (device_manager,
							   device);
	if (!gsd_device)
		return;

	wacom_device = g_hash_table_lookup (self->devices, gsd_device);
	if (!wacom_device)
		return;

	/* Check whether we already know this tool, nothing to do then */
	serial = gdk_device_tool_get_serial (tool);

	/* The wacom driver sends serial-less tools with a serial of
	 * 1, libinput uses 0. No device exists with serial 1, let's reset
	 * it here so everything else works as expected.
	 */
	if (serial == 1)
		serial = 0;

	stylus = cc_tablet_tool_map_lookup_tool (self->tablet_tool_map,
						 wacom_device, serial);

	if (!stylus) {
		id = gdk_device_tool_get_hardware_id (tool);

		/* The wacom driver sends a hw id of 0x2 for stylus and 0xa
		 * for eraser for devices that don't have a true HW id.
		 * Reset those to 0 so we can use the same code-paths
		 * libinput uses.
		 * The touch ID is 0x3, let's ignore that because we don't
		 * have a touch tool and it only happens when the wacom
		 * driver handles the touch device.
		 */
		if (id == 0x2 || id == 0xa)
			id = 0;
		else if (id == 0x3)
			return;

		stylus = cc_wacom_tool_new (serial, id, wacom_device);
		if (!stylus)
			return;
        }

	add_stylus (self, stylus);

	update_highlighted_stylus (self, stylus);

	cc_tablet_tool_map_add_relation (self->tablet_tool_map,
					 wacom_device, stylus);
}

static void
on_stylus_proximity_cb (CcWacomPanel     *self,
			double            x,
			double            y,
			GtkGestureStylus *gesture)
{
	GdkDevice *device;
	GdkDeviceTool *tool;

	device = gtk_event_controller_get_current_event_device (GTK_EVENT_CONTROLLER (gesture));
	tool = gtk_gesture_stylus_get_device_tool (gesture);
	update_current_tool (self, device, tool);
}

static gboolean
show_mock_stylus_cb (gpointer user_data)
{
	CcWacomPanel *self = user_data;
	GList *device_list;
	CcWacomDevice *wacom_device;
	CcWacomTool *stylus;

	self->mock_stylus_id = 0;

	device_list = g_hash_table_get_values (self->devices);
	if (device_list == NULL) {
		g_warning ("Could not create fake stylus event because could not find tablet device");
		return G_SOURCE_REMOVE;
	}

	wacom_device = device_list->data;
	g_list_free (device_list);

	stylus = cc_wacom_tool_new (0, 0, wacom_device);
	add_stylus (self, stylus);
	update_highlighted_stylus (self, stylus);
	cc_tablet_tool_map_add_relation (self->tablet_tool_map,
					 wacom_device, stylus);

	return G_SOURCE_REMOVE;
}

static void
cc_wacom_panel_constructed (GObject *object)
{
	CcWacomPanel *self = CC_WACOM_PANEL (object);
	CcShell *shell;

	G_OBJECT_CLASS (cc_wacom_panel_parent_class)->constructed (object);

	/* Add test area button to shell header. */
	shell = cc_panel_get_shell (CC_PANEL (self));

	self->stylus_gesture = gtk_gesture_stylus_new ();
	g_signal_connect_swapped (self->stylus_gesture, "proximity",
                                  G_CALLBACK (on_stylus_proximity_cb), self);
	gtk_widget_add_controller (GTK_WIDGET (shell),
				   GTK_EVENT_CONTROLLER (self->stylus_gesture));

	if (g_getenv ("UMOCKDEV_DIR") != NULL)
		self->mock_stylus_id = g_idle_add (show_mock_stylus_cb, self);
}

static const char *
cc_wacom_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/wacom";
}

static void
cc_wacom_panel_class_init (CcWacomPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = cc_wacom_panel_get_property;
	object_class->set_property = cc_wacom_panel_set_property;
	object_class->dispose = cc_wacom_panel_dispose;
	object_class->constructed = cc_wacom_panel_constructed;

	panel_class->get_help_uri = cc_wacom_panel_get_help_uri;

	g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

	g_type_ensure (CC_TYPE_DRAWING_AREA);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wacom/cc-wacom-panel.ui");

	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, scrollable);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, test_button);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, test_popover);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, test_draw_area);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, tablets);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, styli);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, initial_state_stack);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, panel_empty_state);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, panel_view);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPanel, vadjustment);
}

static void
add_known_device (CcWacomPanel *self,
		  GsdDevice    *gsd_device)
{
	g_autoptr(CcWacomDevice) device = NULL;
	GsdDeviceType device_type;
	g_autoptr(GList) tools = NULL;
	GtkWidget *page;
	gboolean is_remote = FALSE;
	GList *l;

	device_type = gsd_device_get_device_type (gsd_device);

	if ((device_type & GSD_DEVICE_TYPE_TABLET) == 0)
		return;

	if ((device_type &
	     (GSD_DEVICE_TYPE_TOUCHSCREEN |
	      GSD_DEVICE_TYPE_TOUCHPAD)) != 0) {
		return;
	}

	device = cc_wacom_device_new (gsd_device);
	if (!device)
		return;

	if ((device_type & GSD_DEVICE_TYPE_PAD) != 0) {
		/* Remotes like the Wacom ExpressKey Remote are a special case.
		 * They are external pad devices, we want to distinctly show them
		 * in the list. Other pads are mounted on a tablet, which
		 * get their own entries.
		 */
		is_remote = cc_wacom_device_is_remote (device);
		if (!is_remote)
			return;
	}

	g_hash_table_insert (self->devices, gsd_device, device);

	tools = cc_tablet_tool_map_list_tools (self->tablet_tool_map, device);

	for (l = tools; l != NULL; l = l->next) {
		add_stylus (self, l->data);
	}

	if (is_remote)
		page = cc_wacom_ekr_page_new (self, device);
	else
		page = cc_wacom_page_new (self, device);

	gtk_box_append (GTK_BOX (self->tablets), page);
	g_hash_table_insert (self->pages, g_steal_pointer (&device), page);
}

static void
device_removed_cb (CcWacomPanel     *self,
		   GsdDevice        *gsd_device)
{
	CcWacomDevice *device;
	GtkWidget *page;

	device = g_hash_table_lookup (self->devices, gsd_device);
	if (!device)
		return;

	page = g_hash_table_lookup (self->pages, device);
	if (page) {
		g_hash_table_remove (self->pages, device);
		gtk_box_remove (GTK_BOX (self->tablets), page);
	}

	g_hash_table_remove (self->devices, gsd_device);
	check_remove_stylus_pages (self);
	update_test_button (self);
	update_initial_state (self);
}

static void
device_added_cb (CcWacomPanel *self,
		 GsdDevice    *device)
{
	add_known_device (self, device);
	update_test_button (self);
	update_initial_state (self);
}

void
cc_wacom_panel_switch_to_panel (CcWacomPanel *self,
				const char   *panel)
{
	CcShell *shell;
	g_autoptr(GError) error = NULL;

	g_return_if_fail (self);

	shell = cc_panel_get_shell (CC_PANEL (self));
	if (!cc_shell_set_active_panel_from_id (shell, panel, NULL, &error))
		g_warning ("Failed to activate '%s' panel: %s", panel, error->message);
}

static void
got_osd_proxy_cb (GObject      *source_object,
		  GAsyncResult *res,
		  gpointer      data)
{
	g_autoptr(GError)    error = NULL;
	CcWacomPanel        *self;

	self = CC_WACOM_PANEL (data);
	self->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (self->proxy == NULL) {
		g_printerr ("Error creating proxy: %s\n", error->message);
		return;
	}
}

static void
got_input_mapping_proxy_cb (GObject      *source_object,
			    GAsyncResult *res,
			    gpointer      data)
{
	g_autoptr(GError) error = NULL;
	CcWacomPanel *self;

	self = CC_WACOM_PANEL (data);
	self->input_mapping_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (self->input_mapping_proxy == NULL) {
		g_printerr ("Error creating input mapping proxy: %s\n", error->message);
		return;
	}
}

static void
cc_wacom_panel_init (CcWacomPanel *self)
{
	GsdDeviceManager *device_manager;
	g_autoptr(GList) devices = NULL;
	GList *l;
	g_autoptr(GError) error = NULL;

        g_resources_register (cc_wacom_get_resource ());

	gtk_widget_init_template (GTK_WIDGET (self));

	self->tablet_tool_map = cc_tablet_tool_map_new ();

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  "org.gnome.Shell",
				  "/org/gnome/Shell/Wacom",
				  "org.gnome.Shell.Wacom.PadOsd",
				  cc_panel_get_cancellable (CC_PANEL (self)),
				  got_osd_proxy_cb,
				  self);

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  "org.gnome.Shell",
				  "/org/gnome/Mutter/InputMapping",
				  "org.gnome.Mutter.InputMapping",
				  cc_panel_get_cancellable (CC_PANEL (self)),
				  got_input_mapping_proxy_cb,
				  self);

	self->devices = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
	self->pages = g_hash_table_new (NULL, NULL);
	self->stylus_pages = g_hash_table_new (NULL, NULL);

	device_manager = gsd_device_manager_get ();
	g_signal_connect_object (device_manager, "device-added",
				 G_CALLBACK (device_added_cb), self, G_CONNECT_SWAPPED);
	g_signal_connect_object (device_manager, "device-removed",
				 G_CALLBACK (device_removed_cb), self, G_CONNECT_SWAPPED);

	devices = gsd_device_manager_list_devices (device_manager,
						   GSD_DEVICE_TYPE_TABLET);
	for (l = devices; l ; l = l->next)
		add_known_device (self, l->data);

	update_test_button (self);
	update_initial_state (self);
}

GDBusProxy *
cc_wacom_panel_get_gsd_wacom_bus_proxy (CcWacomPanel *self)
{
	g_return_val_if_fail (CC_IS_WACOM_PANEL (self), NULL);

	return self->proxy;
}

GDBusProxy *
cc_wacom_panel_get_input_mapping_bus_proxy (CcWacomPanel *self)
{
	g_return_val_if_fail (CC_IS_WACOM_PANEL (self), NULL);

	return self->input_mapping_proxy;
}
