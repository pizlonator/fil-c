/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <NetworkManager.h>
#include <string.h>

#include "ce-page.h"
#include "ce-page-ethernet.h"
#include "ce-page-8021x-security.h"
#include "nma-ws.h"

struct _CEPage8021xSecurity {
        AdwBin parent;

        GtkBox      *box;
        GtkSwitch   *enable_8021x_switch;
        GtkLabel    *security_label;

        NMConnection *connection;
        NMAWs8021x *security;
        GtkSizeGroup *group;
        gboolean initial_have_8021x;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPage8021xSecurity, ce_page_8021x_security, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

static void
enable_toggled (CEPage8021xSecurity *self)
{
	gtk_widget_set_sensitive (GTK_WIDGET (self->security), gtk_switch_get_active (self->enable_8021x_switch));
	ce_page_changed (CE_PAGE (self));
}

static void
security_item_changed_cb (CEPage8021xSecurity *self)
{
        ce_page_changed (CE_PAGE (self));
}

static void
finish_setup (CEPage8021xSecurity *self, gpointer unused, GError *error, gpointer user_data)
{
	if (error)
		return;

	self->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	self->security = nma_ws_802_1x_new (self->connection, FALSE, FALSE);
	if (!self->security) {
		g_warning ("Could not load 802.1x user interface.");
		return;
	}

	g_signal_connect_object (NMA_WS (self->security), "ws-changed", G_CALLBACK (security_item_changed_cb), self, G_CONNECT_SWAPPED);
	if (gtk_widget_get_parent (GTK_WIDGET (self->security)))
		gtk_box_remove (self->box, GTK_WIDGET (self->security));

	gtk_switch_set_active (self->enable_8021x_switch, self->initial_have_8021x);
	g_signal_connect_object (self->enable_8021x_switch, "notify::active", G_CALLBACK (enable_toggled), self, G_CONNECT_SWAPPED);
	gtk_widget_set_sensitive (GTK_WIDGET (self->security), self->initial_have_8021x);

	gtk_size_group_add_widget (self->group, GTK_WIDGET (self->security_label));
	nma_ws_add_to_size_group (NMA_WS (self->security), self->group);

	gtk_box_append (self->box, GTK_WIDGET (self->security));

}

static const gchar *
ce_page_8021x_security_get_security_setting (CEPage *page)
{
	CEPage8021xSecurity *self = CE_PAGE_8021X_SECURITY (page);

	if (self->initial_have_8021x)
                return NM_SETTING_802_1X_SETTING_NAME;

        return NULL;
}

static const gchar *
ce_page_8021x_security_get_title (CEPage *page)
{
        return _("Security");
}

static gboolean
ce_page_8021x_security_validate (CEPage *cepage, NMConnection *connection, GError **error)
{
	CEPage8021xSecurity *self = CE_PAGE_8021X_SECURITY (cepage);
	gboolean valid = TRUE;

	if (gtk_switch_get_active (self->enable_8021x_switch)) {
		NMSetting *s_8021x;

		/* FIXME: get failed property and error out of wireless security objects */
		valid = nma_ws_validate (NMA_WS (self->security), error);
		if (valid) {
			g_autoptr(NMConnection) tmp_connection = NULL;

			/* Here's a nice hack to work around the fact that ws_802_1x_fill_connection needs wireless setting. */
			tmp_connection = nm_simple_connection_new_clone (connection);
			nm_connection_add_setting (tmp_connection, nm_setting_wireless_new ());

			nma_ws_fill_connection (NMA_WS (self->security), tmp_connection);

			/* NOTE: It is important we create a copy of the settings, as the
			 * secrets might be cleared otherwise.
			 */
			s_8021x = nm_connection_get_setting (tmp_connection, NM_TYPE_SETTING_802_1X);
			nm_connection_add_setting (connection, nm_setting_duplicate (NM_SETTING (s_8021x)));
		}
	} else {
		nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
		valid = TRUE;
	}

	return valid;
}

static void
ce_page_8021x_security_init (CEPage8021xSecurity *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_8021x_security_dispose (GObject *object)
{
	CEPage8021xSecurity *self = CE_PAGE_8021X_SECURITY (object);

        g_clear_object (&self->connection);
        g_clear_pointer ((GtkWidget **) &self->security, gtk_widget_unparent);
        g_clear_object (&self->group);

	G_OBJECT_CLASS (ce_page_8021x_security_parent_class)->dispose (object);
}

static void
ce_page_8021x_security_class_init (CEPage8021xSecurityClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = ce_page_8021x_security_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/8021x-security-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPage8021xSecurity, box);
        gtk_widget_class_bind_template_child (widget_class, CEPage8021xSecurity, enable_8021x_switch);
        gtk_widget_class_bind_template_child (widget_class, CEPage8021xSecurity, security_label);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_security_setting = ce_page_8021x_security_get_security_setting;
        iface->get_title = ce_page_8021x_security_get_title;
	iface->validate = ce_page_8021x_security_validate;
}

CEPage8021xSecurity *
ce_page_8021x_security_new (NMConnection *connection)
{
	CEPage8021xSecurity *self;

        self = CE_PAGE_8021X_SECURITY (g_object_new (ce_page_8021x_security_get_type (), NULL));

        self->connection = g_object_ref (connection);

	if (nm_connection_get_setting_802_1x (connection))
		self->initial_have_8021x = TRUE;

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return self;
}
