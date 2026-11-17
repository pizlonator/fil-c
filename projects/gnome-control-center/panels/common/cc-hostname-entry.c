/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013 Intel, Inc
 * Copyright (C) 2011,2012 Red Hat, Inc
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
 */


#include "cc-common-resources.h"
#include "cc-hostname.h"
#include "cc-hostname-entry.h"
#include "hostname-helper.h"

#include <polkit/polkit.h>

struct _CcHostnameEntry
{
  AdwEntryRow          parent;

  guint                set_hostname_timeout_source_id;
};

G_DEFINE_TYPE (CcHostnameEntry, cc_hostname_entry, ADW_TYPE_ENTRY_ROW)

#define SET_HOSTNAME_TIMEOUT 1

static gboolean
set_hostname_timeout (CcHostnameEntry *self)
{
  const gchar *text = NULL;

  self->set_hostname_timeout_source_id = 0;

  text = gtk_editable_get_text (GTK_EDITABLE (self));
  cc_hostname_set_hostname (cc_hostname_get_default (), text);

  return FALSE;
}

static void
reset_hostname_timeout (CcHostnameEntry *self)
{
  g_clear_handle_id (&self->set_hostname_timeout_source_id, g_source_remove);

  self->set_hostname_timeout_source_id = g_timeout_add_seconds (SET_HOSTNAME_TIMEOUT,
                                                                (GSourceFunc) set_hostname_timeout,
                                                                self);
}

static void
text_changed_cb (CcHostnameEntry *entry)
{
  reset_hostname_timeout (entry);
}

static void
cc_hostname_entry_dispose (GObject *object)
{
  CcHostnameEntry *self = CC_HOSTNAME_ENTRY (object);

  if (self->set_hostname_timeout_source_id)
    {
      g_clear_handle_id (&self->set_hostname_timeout_source_id, g_source_remove);
      set_hostname_timeout (self);
    }

  G_OBJECT_CLASS (cc_hostname_entry_parent_class)->dispose (object);
}

static void
cc_hostname_entry_constructed (GObject *object)
{
  CcHostnameEntry *self = CC_HOSTNAME_ENTRY (object);
  GPermission *permission;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *str = NULL;

  permission = polkit_permission_new_sync ("org.freedesktop.hostname1.set-static-hostname",
                                           NULL, NULL, NULL);

  /* Is hostnamed installed? */
  if (permission == NULL)
    {
      g_debug ("Will not show hostname, hostnamed not installed");

      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);

      return;
    }

  if (g_permission_get_allowed (permission))
    gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
  else
    {
      g_debug ("Not allowed to change the hostname");
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
    }

  str = cc_hostname_get_display_hostname (cc_hostname_get_default ());
  if (str != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self), str);
  else
    gtk_editable_set_text (GTK_EDITABLE (self), "");

  g_signal_connect (self, "apply", G_CALLBACK (text_changed_cb), NULL);

  adw_entry_row_set_show_apply_button (ADW_ENTRY_ROW (self), TRUE);
}

static void
cc_hostname_entry_class_init (CcHostnameEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = cc_hostname_entry_constructed;
  object_class->dispose = cc_hostname_entry_dispose;
}

static void
cc_hostname_entry_init (CcHostnameEntry *self)
{
  g_resources_register (cc_common_get_resource ());
}

CcHostnameEntry *
cc_hostname_entry_new (void)
{
  return g_object_new (CC_TYPE_HOSTNAME_ENTRY, NULL);
}
