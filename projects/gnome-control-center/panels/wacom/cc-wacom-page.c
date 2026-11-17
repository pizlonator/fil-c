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

#ifdef FAKE_AREA
#include <gdk/gdk.h>
#endif /* FAKE_AREA */

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdesktop-enums.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#include "cc-list-row.h"
#include "cc-wacom-device.h"
#include "cc-wacom-button-row.h"
#include "cc-wacom-page.h"
#include "cc-wacom-stylus-page.h"
#include "gsd-enums.h"
#include "calibrator-gui.h"
#include "gsd-input-helper.h"

#include <string.h>

#define MWID(x) (GtkWidget *) gtk_builder_get_object (page->mapping_builder, x)

#define THRESHOLD_MISCLICK	15
#define THRESHOLD_DOUBLECLICK	7

struct _CcWacomPage
{
	GtkBox          parent_instance;

	CcWacomPanel   *panel;
	CcWacomDevice  *stylus;
	CcWacomDevice  *pad;
	CcCalibArea    *area;
	GSettings      *wacom_settings;

	GtkWidget      *tablet_section;
	GtkWidget      *tablet_icon;
	GtkWidget      *tablet_display;
	GtkWidget      *tablet_calibrate;
	GtkWidget      *tablet_map_buttons;
	AdwSwitchRow   *tablet_mode_row;
	AdwSwitchRow   *tablet_left_handed_row;
	AdwSwitchRow   *tablet_aspect_ratio_row;
	GtkWidget      *display_section;

	GnomeRRScreen  *rr_screen;

	/* Button mapping */
	GtkBuilder     *mapping_builder;
	GtkWindow      *button_map;
	GtkListStore   *action_store;

	GCancellable   *cancellable;

	/* To reach other grouped devices */
	GsdDeviceManager *manager;
};

G_DEFINE_TYPE (CcWacomPage, cc_wacom_page, GTK_TYPE_BOX)

/* Different types of layout for the tablet config */
enum {
	LAYOUT_NORMAL,        /* tracking mode, button mapping */
	LAYOUT_REVERSIBLE,    /* tracking mode, button mapping, left-hand orientation */
	LAYOUT_SCREEN        /* button mapping, calibration, display resolution */
};

static int
get_layout_type (CcWacomDevice *device)
{
	int layout;

	if (cc_wacom_device_get_integration_flags (device) &
	    (WACOM_DEVICE_INTEGRATED_DISPLAY | WACOM_DEVICE_INTEGRATED_SYSTEM))
		layout = LAYOUT_SCREEN;
	else if (cc_wacom_device_is_reversible (device))
		layout = LAYOUT_REVERSIBLE;
	else
		layout = LAYOUT_NORMAL;

	return layout;
}

static void
set_calibration (CcWacomDevice  *device,
                 gdouble        *cal,
                 gsize           ncal,
                 GSettings      *settings)
{
	GVariant    *current; /* current calibration */
	GVariant    *array;   /* new calibration */
	g_autofree GVariant   **tmp = NULL;
	gsize        nvalues;
	gint         i;

	current = g_settings_get_value (settings, "area");
	g_variant_get_fixed_array (current, &nvalues, sizeof (gdouble));
	if ((ncal != 4) || (nvalues != 4)) {
		g_warning("Unable to set device calibration property. Got %"G_GSIZE_FORMAT" items to put in %"G_GSIZE_FORMAT" slots; expected %d items.\n", ncal, nvalues, 4);
		return;
	}

	tmp = g_malloc (nvalues * sizeof (GVariant*));
	for (i = 0; i < ncal; i++)
		tmp[i] = g_variant_new_double (cal[i]);

	array = g_variant_new_array (G_VARIANT_TYPE_DOUBLE, tmp, nvalues);
	g_settings_set_value (settings, "area", array);

	g_debug ("Setting area to %f, %f, %f, %f (left/right/top/bottom)",
		 cal[0], cal[1], cal[2], cal[3]);
}

static void
finish_calibration (CcCalibArea *area,
		    gpointer     user_data)
{
	CcWacomPage *page = (CcWacomPage *) user_data;
	XYinfo axis;
	gdouble cal[4];

	if (cc_calib_area_finish (area)) {
		cc_calib_area_get_padding (area, &axis);
		cal[0] = axis.x_min;
		cal[1] = axis.x_max;
		cal[2] = axis.y_min;
		cal[3] = axis.y_max;

		set_calibration (page->stylus,
				 cal, 4, page->wacom_settings);
	} else {
		/* Reset the old values */
		GVariant *old_calibration;

		old_calibration = g_object_get_data (G_OBJECT (page), "old-calibration");
		g_settings_set_value (page->wacom_settings, "area", old_calibration);
		g_object_set_data (G_OBJECT (page), "old-calibration", NULL);
	}

	cc_calib_area_free (area);
	page->area = NULL;
	gtk_widget_set_sensitive (page->tablet_calibrate, TRUE);
}

static gboolean
run_calibration (CcWacomPage *page,
		 GVariant    *old_calibration,
		 gdouble     *cal,
		 GdkMonitor  *monitor)
{
	g_assert (page->area == NULL);

	page->area = cc_calib_area_new (NULL,
					monitor,
					cc_wacom_device_get_device (page->stylus),
					finish_calibration,
					page,
					THRESHOLD_MISCLICK,
					THRESHOLD_DOUBLECLICK);

	g_object_set_data_full (G_OBJECT (page),
				"old-calibration",
				old_calibration,
				(GDestroyNotify) g_variant_unref);

	return FALSE;
}

static GdkMonitor *
find_monitor_at_point (GdkDisplay *display,
		       gint        x,
		       gint        y)
{
	GListModel *monitors;
	int i;

	monitors = gdk_display_get_monitors (display);

	for (i = 0; i < g_list_model_get_n_items (monitors); i++) {
		g_autoptr(GdkMonitor) m = g_list_model_get_item (monitors, i);
		GdkRectangle geometry;

		gdk_monitor_get_geometry (m, &geometry);
		if (gdk_rectangle_contains_point (&geometry, x, y))
			return g_steal_pointer (&m);
	}

	return NULL;
}

static void
calibrate (CcWacomPage *page)
{
	int i;
	GVariant *old_calibration, *array;
	g_autofree GVariant **tmp = NULL;
	g_autofree gdouble *calibration = NULL;
	gsize ncal;
	GdkDisplay *display;
	g_autoptr(GdkMonitor) monitor = NULL;
	g_autoptr(GnomeRRScreen) rr_screen = NULL;
	GnomeRROutput *output;
	g_autoptr(GError) error = NULL;
	GDBusProxy *input_mapping_proxy;
	gint x, y;

	display = gdk_display_get_default ();
	rr_screen = gnome_rr_screen_new (display, &error);
	if (error) {
		g_warning ("Could not connect to display manager: %s", error->message);
		return;
	}

	output = cc_wacom_device_get_output (page->stylus, rr_screen);
	input_mapping_proxy = cc_wacom_panel_get_input_mapping_bus_proxy (page->panel);

	if (output) {
		gnome_rr_output_get_position (output, &x, &y);
		monitor = find_monitor_at_point (display, x, y);
	} else if (input_mapping_proxy) {
		GsdDevice *gsd_device;
		GVariant *mapping;

		gsd_device = cc_wacom_device_get_device (page->stylus);

		if (gsd_device)	{
			mapping = g_dbus_proxy_call_sync (input_mapping_proxy,
							  "GetDeviceMapping",
							  g_variant_new ("(o)", gsd_device_get_device_file (gsd_device)),
							  G_DBUS_CALL_FLAGS_NONE,
							  -1,
							  NULL,
							  NULL);
			if (mapping) {
				gint x, y, width, height;

				g_variant_get (mapping, "((iiii))", &x, &y, &width, &height);
				monitor = find_monitor_at_point (display, x, y);
			}
		}
	}

	if (!monitor) {
		/* The display the tablet should be mapped to could not be located.
		 * This shouldn't happen if the EDID data is good...
		 */
		g_critical("Output associated with the tablet is not connected. Calibration may appear in wrong monitor.");
	}

	old_calibration = g_settings_get_value (page->wacom_settings, "area");
	g_variant_get_fixed_array (old_calibration, &ncal, sizeof (gdouble));

	if (ncal != 4) {
		g_warning("Device calibration property has wrong length. Got %"G_GSIZE_FORMAT" items; expected %d.\n", ncal, 4);
		return;
	}

	calibration = g_new0 (gdouble, ncal);

	/* Reset the current values, to avoid old calibrations
	 * from interfering with the calibration */
	tmp = g_malloc (ncal * sizeof (GVariant*));
	for (i = 0; i < ncal; i++) {
		calibration[i] = 0.0;
		tmp[i] = g_variant_new_double (calibration[i]);
	}

	array = g_variant_new_array (G_VARIANT_TYPE_DOUBLE, tmp, ncal);
	g_settings_set_value (page->wacom_settings, "area", array);

	run_calibration (page, old_calibration, calibration, monitor);
	gtk_widget_set_sensitive (page->tablet_calibrate, FALSE);
}

static void
on_calibrate_activated (CcWacomPage *self)
{
	calibrate (self);
}

/* This avoids us crashing when a newer version of
 * gnome-control-center has been used, and we load up an
 * old one, as the action type if unknown to the old g-c-c */
static gboolean
action_type_is_valid (GDesktopPadButtonAction action)
{
	if (action >= G_N_ELEMENTS (action_table))
		return FALSE;
	return TRUE;
}

static void
create_row_from_button (GtkWidget *list_box,
			guint      button,
			GSettings *settings)
{
	gtk_list_box_append (GTK_LIST_BOX (list_box),
                       cc_wacom_button_row_new (button, settings));
}

static void
setup_button_mapping (CcWacomPage *page)
{
	GDesktopPadButtonAction action;
	CcWacomDevice *pad;
	GtkWidget *list_box;
	guint i, n_buttons;
	GSettings *settings;

	list_box = MWID ("shortcuts_list");
	pad = page->pad;
	n_buttons = cc_wacom_device_get_num_buttons (pad);

	for (i = 0; i < n_buttons; i++) {
		settings = cc_wacom_device_get_button_settings (pad, i);
		if (!settings)
			continue;

		action = g_settings_get_enum (settings, "action");
		if (!action_type_is_valid (action))
			continue;

		create_row_from_button (list_box, i, settings);
	}
}

static void
button_mapping_dialog_closed (CcWacomPage *page)
{
	gtk_window_destroy (GTK_WINDOW (MWID ("button-mapping-dialog")));
	g_clear_object (&page->mapping_builder);
}

static void
show_button_mapping_dialog (CcWacomPage *page)
{
	GtkWidget          *toplevel;
	g_autoptr(GError)   error = NULL;
	GtkWidget          *dialog;

	g_assert (page->mapping_builder == NULL);
	page->mapping_builder = gtk_builder_new ();
	gtk_builder_add_from_resource (page->mapping_builder,
                                       "/org/gnome/control-center/wacom/button-mapping.ui",
                                       &error);

	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_clear_object (&page->mapping_builder);
		return;
	}

	setup_button_mapping (page);

	dialog = MWID ("button-mapping-dialog");
	toplevel = GTK_WIDGET (gtk_widget_get_native (GTK_WIDGET (page)));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	g_signal_connect_object (dialog, "response",
	                         G_CALLBACK (button_mapping_dialog_closed), page, G_CONNECT_SWAPPED);

	gtk_window_present (GTK_WINDOW (dialog));

	page->button_map = GTK_WINDOW (dialog);
	g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &page->button_map);
}

static void
set_osd_visibility_cb (GObject      *source_object,
		       GAsyncResult *res,
		       gpointer      data)
{
	g_autoptr(GError) error = NULL;
	GVariant    *result;
	CcWacomPage *page;

	page = CC_WACOM_PAGE (data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

	if (result == NULL) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_printerr ("Error setting OSD's visibility: %s\n", error->message);
			show_button_mapping_dialog (page);
		} else {
			return;
		}
	}
}

static void
set_osd_visibility (CcWacomPage *page)
{
	GDBusProxy         *proxy;
	GsdDevice          *gsd_device;
	const gchar        *device_path;

	proxy = cc_wacom_panel_get_gsd_wacom_bus_proxy (page->panel);

	/* Pick the first device, the OSD may change later between them */
	gsd_device = cc_wacom_device_get_device (page->pad);

	device_path = gsd_device_get_device_file (gsd_device);

	if (proxy == NULL) {
		show_button_mapping_dialog (page);
		return;
	}

	g_dbus_proxy_call (proxy,
			   "Show",
			   g_variant_new ("(ob)", device_path, TRUE),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   page->cancellable,
			   set_osd_visibility_cb,
			   page);
}

static void
on_map_buttons_activated (CcWacomPage *self)
{
	set_osd_visibility (self);
}

static void
on_display_selected (CcWacomPage *page)
{
	GListModel *list;
	g_autoptr (GObject) obj = NULL;
	GVariant *variant;
	gint idx;

	list = adw_combo_row_get_model (ADW_COMBO_ROW (page->tablet_display));
	idx = adw_combo_row_get_selected (ADW_COMBO_ROW (page->tablet_display));
	obj = g_list_model_get_item (list, idx);

	variant = g_object_get_data (obj, "value-output");

	if (variant)
		g_settings_set_value (page->wacom_settings, "output", g_variant_ref (variant));
	else
		g_settings_reset (page->wacom_settings, "output");

	gtk_widget_set_sensitive (page->tablet_calibrate, variant == NULL);
}

/* Boilerplate code goes below */

static void
cc_wacom_page_get_property (GObject    *object,
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
cc_wacom_page_set_property (GObject      *object,
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
cc_wacom_page_dispose (GObject *object)
{
	CcWacomPage *self = CC_WACOM_PAGE (object);

	g_cancellable_cancel (self->cancellable);
	g_clear_object (&self->cancellable);
	g_clear_pointer (&self->area, cc_calib_area_free);
	g_clear_pointer (&self->button_map, gtk_window_destroy);
	g_clear_object (&self->pad);
	g_clear_object (&self->rr_screen);

	self->panel = NULL;

	G_OBJECT_CLASS (cc_wacom_page_parent_class)->dispose (object);
}

static void
cc_wacom_page_class_init (CcWacomPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = cc_wacom_page_get_property;
	object_class->set_property = cc_wacom_page_set_property;
	object_class->dispose = cc_wacom_page_dispose;

	g_type_ensure (CC_TYPE_LIST_ROW);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wacom/cc-wacom-page.ui");

	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, tablet_section);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, tablet_icon);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, tablet_display);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, tablet_calibrate);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, tablet_map_buttons);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, tablet_mode_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, tablet_left_handed_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, tablet_aspect_ratio_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomPage, display_section);

	gtk_widget_class_bind_template_callback (widget_class, on_map_buttons_activated);
	gtk_widget_class_bind_template_callback (widget_class, on_calibrate_activated);
	gtk_widget_class_bind_template_callback (widget_class, on_display_selected);
}

static void
update_displays_model (CcWacomPage *page)
{
	g_autoptr (GtkStringList) list = NULL;
	GnomeRROutput **outputs, *cur_output;
	int i, idx = 0, cur = -1, automatic_item = -1;
	g_autoptr (GObject) obj = NULL;
	GVariant *variant;

	outputs = gnome_rr_screen_list_outputs (page->rr_screen);
	list = gtk_string_list_new (NULL);
	cur_output = cc_wacom_device_get_output (page->stylus,
						 page->rr_screen);

	for (i = 0; outputs[i] != NULL; i++) {
		GnomeRROutput *output = outputs[i];
		GnomeRRCrtc *crtc = gnome_rr_output_get_crtc (output);
		g_autofree gchar *text = NULL;
		g_autofree gchar *vendor = NULL;
		g_autofree gchar *product = NULL;
		g_autofree gchar *serial = NULL;
		const gchar *name, *disp_name;

		/* Output is turned on? */
		if (!crtc || gnome_rr_crtc_get_current_mode (crtc) == NULL)
			continue;

		if (output == cur_output)
			cur = idx;

		name = gnome_rr_output_get_name (output);
		disp_name = gnome_rr_output_get_display_name (output);
		text = g_strdup_printf ("%s (%s)", name, disp_name);

		gnome_rr_output_get_ids_from_edid (output,
						   &vendor,
						   &product,
						   &serial);
		variant = g_variant_new_strv ((const gchar *[]) { vendor, product, serial, name }, 4);

		gtk_string_list_append (list, text);
		obj = g_list_model_get_item (G_LIST_MODEL (list), idx);
		g_object_set_data_full (G_OBJECT (obj), "value-output",
					variant, (GDestroyNotify) g_variant_unref);
		idx++;
	}

	/* All displays item */
	gtk_string_list_append (list, _("All Displays"));
	variant = g_variant_new_strv ((const gchar *[]) { "", "", "" }, 3);
	obj = g_list_model_get_item (G_LIST_MODEL (list), idx);
	g_object_set_data_full (G_OBJECT (obj), "value-output",
				variant, (GDestroyNotify) g_variant_unref);
	if (cur_output == NULL)
		cur = idx;

	/* "Automatic" item */
	if (get_layout_type (page->stylus) == LAYOUT_SCREEN) {
		g_autoptr (GVariant) user_value = NULL;

		idx++;
		gtk_string_list_append (list, _("Automatic"));
		automatic_item = idx;

		user_value = g_settings_get_user_value (page->wacom_settings, "output");
		if (!user_value)
			cur = idx;
	}

	g_signal_handlers_block_by_func (page->tablet_display, on_display_selected, page);
	adw_combo_row_set_model (ADW_COMBO_ROW (page->tablet_display), G_LIST_MODEL (list));
	adw_combo_row_set_selected (ADW_COMBO_ROW (page->tablet_display), cur);
	g_signal_handlers_unblock_by_func (page->tablet_display, on_display_selected, page);

	gtk_widget_set_sensitive (page->tablet_calibrate, cur == automatic_item);
}

static void
cc_wacom_page_init (CcWacomPage *page)
{
	g_autoptr (GError) error = NULL;

	gtk_widget_init_template (GTK_WIDGET (page));
	page->rr_screen = gnome_rr_screen_new (gdk_display_get_default (), &error);

	if (error)
		g_warning ("Could not get RR screen: %s", error->message);

	g_signal_connect_object (page->rr_screen, "changed",
				 G_CALLBACK (update_displays_model),
				 page, G_CONNECT_SWAPPED);
}

static void
set_icon_name (CcWacomPage *page,
	       GtkWidget   *widget,
	       const char  *icon_name)
{
	g_autofree gchar *resource = NULL;

	resource = g_strdup_printf ("/org/gnome/control-center/wacom/%s.svg", icon_name);
	gtk_picture_set_resource (GTK_PICTURE (widget), resource);
}

static gboolean
has_monitor (CcWacomPage *page)
{
	WacomIntegrationFlags integration_flags;

	integration_flags = cc_wacom_device_get_integration_flags (page->stylus);

	return ((integration_flags &
		 (WACOM_DEVICE_INTEGRATED_DISPLAY | WACOM_DEVICE_INTEGRATED_SYSTEM)) != 0);
}

static void
update_pad_availability (CcWacomPage *page)
{
	gboolean is_fallback = cc_wacom_device_is_fallback (page->stylus);

	gtk_widget_set_visible (page->tablet_map_buttons, !is_fallback && page->pad != NULL);
}

static void
check_add_pad (CcWacomPage *page,
	       GsdDevice   *gsd_device)
{
	const gchar *stylus_vendor, *stylus_product;
	const gchar *pad_vendor, *pad_product;
	GsdDevice *stylus_device;

	if ((gsd_device_get_device_type (gsd_device) & GSD_DEVICE_TYPE_PAD) == 0)
		return;

	stylus_device = cc_wacom_device_get_device (page->stylus);
	gsd_device_get_device_ids (cc_wacom_device_get_device (page->stylus),
				   &stylus_vendor, &stylus_product);
	gsd_device_get_device_ids (gsd_device, &pad_vendor, &pad_product);

	if (!gsd_device_shares_group (stylus_device, gsd_device) ||
	    g_strcmp0 (stylus_vendor, pad_vendor) != 0 ||
	    g_strcmp0 (stylus_product, pad_product) != 0)
		return;

	page->pad = cc_wacom_device_new (gsd_device);
	if (page->pad)
		update_pad_availability (page);
}

static void
check_remove_pad (CcWacomPage *page,
		  GsdDevice   *gsd_device)
{
	if ((gsd_device_get_device_type (gsd_device) & GSD_DEVICE_TYPE_PAD) == 0)
		return;

	if (cc_wacom_device_get_device (page->pad) == gsd_device) {
		g_clear_object (&page->pad);
		update_pad_availability (page);
	}
}

static GVariant *
tablet_mode_bind_set (const GValue       *value,
		      const GVariantType *expected_type,
		      gpointer            user_data)
{
	gboolean setting;

	setting = g_value_get_boolean (value);

	return g_variant_new_string (setting ? "absolute" : "relative");
}

static gboolean
tablet_mode_bind_get (GValue   *value,
		      GVariant *variant,
		      gpointer  user_data)
{
	g_value_set_boolean (value,
			     g_strcmp0 (g_variant_get_string (variant, NULL),
					"absolute") == 0);
	return TRUE;
}

GtkWidget *
cc_wacom_page_new (CcWacomPanel  *panel,
		   CcWacomDevice *stylus)
{
	g_autoptr (GList) pads = NULL;
	CcWacomPage *page;
	GList *l;

	g_return_val_if_fail (CC_IS_WACOM_DEVICE (stylus), NULL);

	page = g_object_new (CC_TYPE_WACOM_PAGE, NULL);

	page->panel = panel;
	page->stylus = stylus;

	gtk_widget_set_visible (GTK_WIDGET (page->tablet_left_handed_row),
				get_layout_type (stylus) == LAYOUT_REVERSIBLE);
	gtk_widget_set_visible (page->tablet_calibrate,
				get_layout_type (stylus) == LAYOUT_SCREEN);

	/* FIXME move this to construct */
	page->wacom_settings  = cc_wacom_device_get_settings (stylus);

	/* Tablet name */
	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (page->tablet_section),
					 cc_wacom_device_get_name (stylus));
	adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (page->tablet_section),
					       cc_wacom_device_get_description (stylus));

	g_settings_bind_with_mapping (page->wacom_settings, "mapping",
				      page->tablet_mode_row, "active",
				      G_SETTINGS_BIND_DEFAULT,
				      tablet_mode_bind_get,
				      tablet_mode_bind_set,
				      NULL, NULL);
	g_settings_bind_with_mapping (page->wacom_settings, "mapping",
				      page->display_section, "sensitive",
				      G_SETTINGS_BIND_DEFAULT,
				      tablet_mode_bind_get,
				      tablet_mode_bind_set,
				      NULL, NULL);
	g_settings_bind (page->wacom_settings, "left-handed",
			 page->tablet_left_handed_row, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (page->wacom_settings, "keep-aspect",
			 page->tablet_aspect_ratio_row, "active",
			 G_SETTINGS_BIND_DEFAULT);

	/* Tablet icon */
	set_icon_name (page, page->tablet_icon, cc_wacom_device_get_icon_name (stylus));

	/* Listen to changes in related/paired pads */
	page->manager = gsd_device_manager_get ();
	g_signal_connect_object (G_OBJECT (page->manager), "device-added",
				 G_CALLBACK (check_add_pad), page,
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (G_OBJECT (page->manager), "device-removed",
				 G_CALLBACK (check_remove_pad), page,
				 G_CONNECT_SWAPPED);

	pads = gsd_device_manager_list_devices (page->manager, GSD_DEVICE_TYPE_PAD);
	for (l = pads; l ; l = l->next)
		check_add_pad (page, l->data);

	update_pad_availability (page);
	update_displays_model (page);

	return GTK_WIDGET (page);
}

void
cc_wacom_page_calibrate (CcWacomPage *page)
{
	g_return_if_fail (CC_IS_WACOM_PAGE (page));

	calibrate (page);
}

gboolean
cc_wacom_page_can_calibrate (CcWacomPage *page)
{
	g_return_val_if_fail (CC_IS_WACOM_PAGE (page),
			      FALSE);

	return has_monitor (page);
}
