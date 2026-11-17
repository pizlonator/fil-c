/*
 * Copyright © 2022 Red Hat, Inc.
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
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include "cc-wacom-ekr-page.h"
#include "cc-wacom-panel.h"

struct _CcWacomEkrPage
{
	GtkBox parent_instance;
	GtkWidget *ekr_section;
	GtkWidget *ekr_icon;
	GtkWidget *ekr_map_buttons;
	CcWacomPanel *panel;
	CcWacomDevice *device;
};

enum {
	PROP_0,
	PROP_PANEL,
	PROP_DEVICE,
	N_PROPS,
};

static GParamSpec *props[N_PROPS] = { 0, };

G_DEFINE_TYPE (CcWacomEkrPage, cc_wacom_ekr_page, GTK_TYPE_BOX)

static void
set_osd_visibility_cb (GObject      *source_object,
		       GAsyncResult *res,
		       gpointer      data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) result = NULL;

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

	if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		g_warning ("Error invoking pad button mapping OSK: %s\n", error->message);
}

static void
set_osd_visibility (CcWacomEkrPage *page)
{
	GDBusProxy *proxy;
	GsdDevice *gsd_device;
	const gchar *device_path;

	proxy = cc_wacom_panel_get_gsd_wacom_bus_proxy (page->panel);

	if (proxy == NULL) {
		g_warning ("Wacom D-Bus interface is not available");
		return;
	}

	gsd_device = cc_wacom_device_get_device (page->device);
	device_path = gsd_device_get_device_file (gsd_device);

	g_dbus_proxy_call (proxy,
			   "Show",
			   g_variant_new ("(ob)", device_path, TRUE),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   set_osd_visibility_cb,
			   page);
}

static void
on_map_buttons_activated (CcWacomEkrPage *self)
{
	set_osd_visibility (self);
}

static void
cc_wacom_ekr_page_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	CcWacomEkrPage *page = CC_WACOM_EKR_PAGE (object);

	switch (prop_id) {
	case PROP_PANEL:
		g_value_set_object (value, page->panel);
		break;
	case PROP_DEVICE:
		g_value_set_object (value, page->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cc_wacom_ekr_page_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	CcWacomEkrPage *page = CC_WACOM_EKR_PAGE (object);

	switch (prop_id) {
	case PROP_PANEL:
		page->panel = g_value_get_object (value);
		break;
	case PROP_DEVICE:
		page->device = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cc_wacom_ekr_page_constructed (GObject *object)
{
	CcWacomEkrPage *page = CC_WACOM_EKR_PAGE (object);

	G_OBJECT_CLASS (cc_wacom_ekr_page_parent_class)->constructed (object);

	adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (page->ekr_section),
					 cc_wacom_device_get_name (page->device));
}

static void
cc_wacom_ekr_page_class_init (CcWacomEkrPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = cc_wacom_ekr_page_get_property;
	object_class->set_property = cc_wacom_ekr_page_set_property;
	object_class->constructed = cc_wacom_ekr_page_constructed;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wacom/cc-wacom-ekr-page.ui");

	gtk_widget_class_bind_template_child (widget_class, CcWacomEkrPage, ekr_section);
	gtk_widget_class_bind_template_child (widget_class, CcWacomEkrPage, ekr_icon);
	gtk_widget_class_bind_template_child (widget_class, CcWacomEkrPage, ekr_map_buttons);

	gtk_widget_class_bind_template_callback (widget_class, on_map_buttons_activated);

	props[PROP_PANEL] = g_param_spec_object ("panel",
						 "panel",
						 "panel",
						 CC_TYPE_WACOM_PANEL,
						 G_PARAM_READWRITE |
						 G_PARAM_CONSTRUCT_ONLY);

	props[PROP_DEVICE] = g_param_spec_object ("device",
						  "device",
						  "device",
						  CC_TYPE_WACOM_DEVICE,
						  G_PARAM_READWRITE |
						  G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
cc_wacom_ekr_page_init (CcWacomEkrPage *page)
{
	gtk_widget_init_template (GTK_WIDGET (page));
}

GtkWidget *
cc_wacom_ekr_page_new (CcWacomPanel  *panel,
		       CcWacomDevice *ekr)
{
	return g_object_new (CC_TYPE_WACOM_EKR_PAGE,
			     "panel", panel,
			     "device", ekr,
			     NULL);
}
