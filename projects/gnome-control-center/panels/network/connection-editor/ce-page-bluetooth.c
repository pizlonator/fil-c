/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2023 Red Hat, Inc
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "ce-page.h"
#include "ce-page-bluetooth.h"
#include "ui-helpers.h"

struct _CEPageBluetooth
{
        GtkGrid parent;

        GtkEntry        *name_entry;

        NMSettingConnection *setting_connection;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageBluetooth, ce_page_bluetooth, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

static void
connect_bluetooth_page (CEPageBluetooth *self)
{
        const gchar *name;

        name = nm_setting_connection_get_id (self->setting_connection);
        gtk_editable_set_text (GTK_EDITABLE (self->name_entry), name);

        g_signal_connect_object (self->name_entry, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
}

static const gchar *
ce_page_bluetooth_get_title (CEPage *page)
{
        return _("Identity");
}

static gboolean
ce_page_bluetooth_validate (CEPage        *page,
                            NMConnection  *connection,
                            GError       **error)
{
        CEPageBluetooth *self = CE_PAGE_BLUETOOTH (page);

        g_object_set (self->setting_connection,
                      NM_SETTING_CONNECTION_ID, gtk_editable_get_text (GTK_EDITABLE (self->name_entry)),
                      NULL);

        return nm_setting_verify (NM_SETTING (self->setting_connection), NULL, error);
}

static void
ce_page_bluetooth_init (CEPageBluetooth *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_bluetooth_class_init (CEPageBluetoothClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/bluetooth-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageBluetooth, name_entry);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_title = ce_page_bluetooth_get_title;
        iface->validate = ce_page_bluetooth_validate;
}

CEPageBluetooth *
ce_page_bluetooth_new (NMConnection     *connection)
{
        CEPageBluetooth *self;

        self = CE_PAGE_BLUETOOTH (g_object_new (ce_page_bluetooth_get_type (), NULL));

        self->setting_connection = nm_connection_get_setting_connection (connection);

        connect_bluetooth_page (self);

        return self;
}
