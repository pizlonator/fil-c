/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
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

#pragma once

#include <glib-object.h>

#include <NetworkManager.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

G_DECLARE_INTERFACE (CEPage, ce_page, CE, PAGE, GObject)

struct _CEPageInterface
{
        GTypeInterface g_iface;

        gboolean     (*validate)             (CEPage *page, NMConnection *connection, GError **error);
        const gchar *(*get_title)            (CEPage *page);
        const gchar *(*get_security_setting) (CEPage *page);
};

const gchar *ce_page_get_title       (CEPage           *page);
const gchar *ce_page_get_security_setting (CEPage           *page);
gboolean     ce_page_validate        (CEPage           *page,
                                      NMConnection     *connection,
                                      GError          **error);
void         ce_page_changed         (CEPage           *page);
void         ce_page_complete_init   (CEPage           *page,
                                      NMConnection     *connection,
                                      const gchar      *setting_name,
                                      GVariant         *variant,
                                      GError           *error);

gchar      **ce_page_get_mac_list    (NMClient         *client,
                                      GType             device_type,
                                      const gchar      *mac_property);
void         ce_page_setup_mac_combo (GtkComboBoxText  *combo,
                                      const gchar      *current_mac,
                                      gchar           **mac_list);
void         ce_page_setup_cloned_mac_combo (GtkComboBoxText *combo,
                                             const char      *current,
                                             gboolean         is_wifi);
gint         ce_get_property_default (NMSetting        *setting,
                                      const gchar      *property_name);
gboolean     ce_page_address_is_valid (const gchar *addr);
gchar       *ce_page_trim_address (const gchar *addr);
char        *ce_page_cloned_mac_get (GtkComboBoxText *combo);
gboolean     ce_page_cloned_mac_combo_valid (GtkComboBoxText  *combo);

typedef enum {
        NAME_FORMAT_TYPE,
        NAME_FORMAT_PROFILE
} NameFormat;

gchar * ce_page_get_next_available_name (const GPtrArray *connections,
                                         NameFormat format,
                                         const gchar *type_name);

G_END_DECLS
