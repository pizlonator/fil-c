/* GStreamer
 * Copyright (C) <2011> Wim Taymans <wim.taymans@gmail.com>
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

/**
 * SECTION:gstnetaddressmeta
 * @title: GstNetAddressMeta
 * @short_description: Network address metadata
 *
 * #GstNetAddressMeta can be used to store a network address (a #GSocketAddress)
 * in a #GstBuffer so that it network elements can track the to and from address
 * of the buffer.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstnetaddressmeta.h"

static gboolean
net_address_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstNetAddressMeta *nmeta = (GstNetAddressMeta *) meta;

  nmeta->addr = NULL;

  return TRUE;
}

static gboolean
net_address_meta_transform (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstNetAddressMeta *smeta, *dmeta;
  smeta = (GstNetAddressMeta *) meta;

  /* we always copy no matter what transform */
  dmeta = gst_buffer_add_net_address_meta (transbuf, smeta->addr);
  if (!dmeta)
    return FALSE;

  return TRUE;
}

static void
net_address_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstNetAddressMeta *nmeta = (GstNetAddressMeta *) meta;

  if (nmeta->addr)
    g_object_unref (nmeta->addr);
  nmeta->addr = NULL;
}

GType
gst_net_address_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { "origin", NULL };

  if (g_once_init_enter_pointer (&type)) {
    GType _type = gst_meta_api_type_register ("GstNetAddressMetaAPI", tags);
    g_once_init_leave_pointer (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_net_address_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter_pointer ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi = gst_meta_register (GST_NET_ADDRESS_META_API_TYPE,
        "GstNetAddressMeta",
        sizeof (GstNetAddressMeta),
        net_address_meta_init,
        net_address_meta_free, net_address_meta_transform);
    g_once_init_leave_pointer ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;
}

/**
 * gst_buffer_add_net_address_meta:
 * @buffer: a #GstBuffer
 * @addr: a @GSocketAddress to connect to @buffer
 *
 * Attaches @addr as metadata in a #GstNetAddressMeta to @buffer.
 *
 * Returns: (transfer none): a #GstNetAddressMeta connected to @buffer
 */
GstNetAddressMeta *
gst_buffer_add_net_address_meta (GstBuffer * buffer, GSocketAddress * addr)
{
  GstNetAddressMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (addr), NULL);

  meta =
      (GstNetAddressMeta *) gst_buffer_add_meta (buffer,
      GST_NET_ADDRESS_META_INFO, NULL);

  meta->addr = g_object_ref (addr);

  return meta;
}

/**
 * gst_buffer_get_net_address_meta:
 * @buffer: a #GstBuffer
 *
 * Find the #GstNetAddressMeta on @buffer.
 *
 * Returns: (transfer none) (nullable): the #GstNetAddressMeta or %NULL when there
 * is no such metadata on @buffer.
 */
GstNetAddressMeta *
gst_buffer_get_net_address_meta (GstBuffer * buffer)
{
  return (GstNetAddressMeta *)
      gst_buffer_get_meta (buffer, GST_NET_ADDRESS_META_API_TYPE);
}
