/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
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

#include <NetworkManager.h>

#include "ce-ip-address-entry.h"

struct _CEIPAddressEntry
{
  GtkEntry parent_instance;

  int family;
};

static void ce_ip_address_entry_editable_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CEIPAddressEntry, ce_ip_address_entry, GTK_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                ce_ip_address_entry_editable_init))

static void
ce_ip_address_entry_changed (GtkEditable *editable)
{
  CEIPAddressEntry *self = CE_IP_ADDRESS_ENTRY (editable);

  if (ce_ip_address_entry_is_valid (self))
    gtk_widget_remove_css_class (GTK_WIDGET (self), "error");
  else
    gtk_widget_add_css_class (GTK_WIDGET (self), "error");
}

static void
ce_ip_address_entry_init (CEIPAddressEntry *self)
{
}

static void
ce_ip_address_entry_editable_init (GtkEditableInterface *iface)
{
  iface->changed = ce_ip_address_entry_changed;
}

static void
ce_ip_address_entry_class_init (CEIPAddressEntryClass *klass)
{
}

CEIPAddressEntry *
ce_ip_address_entry_new (int family)
{
  CEIPAddressEntry *self;

  self = CE_IP_ADDRESS_ENTRY (g_object_new (ce_ip_address_entry_get_type (), NULL));
  self->family = family;

  return self;
}

gboolean
ce_ip_address_entry_is_empty (CEIPAddressEntry *self)
{
  const gchar *text;

  g_return_val_if_fail (CE_IS_IP_ADDRESS_ENTRY (self), FALSE);

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  return text[0] == '\0';
}

gboolean
ce_ip_address_entry_is_valid (CEIPAddressEntry *self)
{
  const gchar *text;

  g_return_val_if_fail (CE_IS_IP_ADDRESS_ENTRY (self), FALSE);

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  return text[0] == '\0' || nm_utils_ipaddr_valid (self->family, text);
}
