/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __KSHELPERS_H__
#define __KSHELPERS_H__

#include <glib.h>
#include <windows.h>
#include <mmsystem.h>
#include <ks.h>

G_BEGIN_DECLS

typedef struct _KsDeviceEntry KsDeviceEntry;

struct _KsDeviceEntry
{
  guint index;
  gchar * name;
  gchar * path;
};

gboolean ks_is_valid_handle (HANDLE h);

GList * ks_enumerate_devices (const GUID * devtype, const GUID * direction_category);
void ks_device_entry_free (KsDeviceEntry * entry);
void ks_device_list_free (GList * devices);

gboolean ks_filter_get_pin_property (HANDLE filter_handle, gulong pin_id, GUID prop_set, gulong prop_id, gpointer value, gulong value_size, gulong * error);
gboolean ks_filter_get_pin_property_multi (HANDLE filter_handle, gulong pin_id, GUID prop_set, gulong prop_id, KSMULTIPLE_ITEM ** items, gulong * error);

gboolean ks_object_query_property (HANDLE handle, GUID prop_set, gulong prop_id, gulong prop_flags, gpointer * value, gulong * value_size, gulong * error);
gboolean ks_object_get_property (HANDLE handle, GUID prop_set, gulong prop_id, gpointer * value, gulong * value_size, gulong * error);
gboolean ks_object_set_property (HANDLE handle, GUID prop_set, gulong prop_id, gpointer value, gulong value_size, gulong * error);

gboolean ks_object_get_supported_property_sets (HANDLE handle, GUID ** propsets, gulong * len);

gboolean ks_object_set_connection_state (HANDLE handle, KSSTATE state, gulong * error);

gchar * ks_guid_to_string (const GUID * guid);
const gchar * ks_state_to_string (KSSTATE state);
gchar * ks_options_flags_to_string (gulong flags);
gchar * ks_property_set_to_string (const GUID * guid);

G_END_DECLS

#endif /* __KSHELPERS_H__ */
