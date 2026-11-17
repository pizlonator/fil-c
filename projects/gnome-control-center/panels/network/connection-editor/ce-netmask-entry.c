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

#include <arpa/inet.h>
#include <NetworkManager.h>

#include "ce-netmask-entry.h"

struct _CENetmaskEntry
{
  GtkEntry parent_instance;
};

static void ce_netmask_entry_editable_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CENetmaskEntry, ce_netmask_entry, GTK_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                ce_netmask_entry_editable_init))

static gboolean
parse_netmask (const char *str, guint32 *prefix)
{
  struct in_addr tmp_addr;
  glong tmp_prefix;

  /* Is it a prefix? */
  errno = 0;
  if (!strchr (str, '.'))
    {
      tmp_prefix = strtol (str, NULL, 10);
      if (!errno && tmp_prefix >= 0 && tmp_prefix <= 32)
        {
          if (prefix != NULL)
            *prefix = tmp_prefix;
          return TRUE;
        }
    }

  /* Is it a netmask? */
  if (inet_pton (AF_INET, str, &tmp_addr) > 0)
    {
      if (prefix != NULL)
        *prefix = nm_utils_ip4_netmask_to_prefix (tmp_addr.s_addr);
      return TRUE;
    }

  return FALSE;
}

static void
ce_netmask_entry_changed (GtkEditable *editable)
{
  CENetmaskEntry *self = CE_NETMASK_ENTRY (editable);

  if (ce_netmask_entry_is_valid (self))
    gtk_widget_remove_css_class (GTK_WIDGET (self), "error");
  else
    gtk_widget_add_css_class (GTK_WIDGET (self), "error");
}

static void
ce_netmask_entry_init (CENetmaskEntry *self)
{
}

static void
ce_netmask_entry_editable_init (GtkEditableInterface *iface)
{
  iface->changed = ce_netmask_entry_changed;
}

static void
ce_netmask_entry_class_init (CENetmaskEntryClass *klass)
{
}

CENetmaskEntry *
ce_netmask_entry_new (void)
{
  return CE_NETMASK_ENTRY (g_object_new (ce_netmask_entry_get_type (), NULL));
}

gboolean
ce_netmask_entry_is_empty (CENetmaskEntry *self)
{
  const gchar *text;

  g_return_val_if_fail (CE_IS_NETMASK_ENTRY (self), FALSE);

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  return text[0] == '\0';
}

gboolean
ce_netmask_entry_is_valid (CENetmaskEntry *self)
{
  const gchar *text;

  g_return_val_if_fail (CE_IS_NETMASK_ENTRY (self), FALSE);

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  return text[0] == '\0' || parse_netmask (text, NULL);
}

guint32
ce_netmask_entry_get_prefix (CENetmaskEntry *self)
{
  const gchar *text;
  guint32 prefix = 0;

  g_return_val_if_fail (CE_IS_NETMASK_ENTRY (self), 0);

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  parse_netmask (text, &prefix);

  return prefix;
}
