/* GStreamer
 * Copyright (C) <2014> William Manley <will@williammanley.net>
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
 * SECTION:gstnetcontrolmessagemeta
 * @title: GstNetControlMessageMeta
 * @short_description: Network Control Message Meta
 *
 * #GstNetControlMessageMeta can be used to store control messages (ancillary
 * data) which was received with or is to be sent alongside the buffer data.
 * When used with socket sinks and sources which understand this meta it allows
 * sending and receiving ancillary data such as unix credentials (See
 * #GUnixCredentialsMessage) and Unix file descriptions (See #GUnixFDMessage).
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstnetcontrolmessagemeta.h"

static gboolean
net_control_message_meta_init (GstMeta * meta, gpointer params,
    GstBuffer * buffer)
{
  GstNetControlMessageMeta *nmeta = (GstNetControlMessageMeta *) meta;

  nmeta->message = NULL;

  return TRUE;
}

static gboolean
net_control_message_meta_transform (GstBuffer * transbuf, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstNetControlMessageMeta *smeta, *dmeta;
  smeta = (GstNetControlMessageMeta *) meta;

  /* we always copy no matter what transform */
  dmeta = gst_buffer_add_net_control_message_meta (transbuf, smeta->message);
  if (!dmeta)
    return FALSE;

  return TRUE;
}

static void
net_control_message_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstNetControlMessageMeta *nmeta = (GstNetControlMessageMeta *) meta;

  if (nmeta->message)
    g_object_unref (nmeta->message);
  nmeta->message = NULL;
}

GType
gst_net_control_message_meta_api_get_type (void)
{
  static GType type;
  static const gchar *tags[] = { "origin", NULL };

  if (g_once_init_enter_pointer (&type)) {
    GType _type =
        gst_meta_api_type_register ("GstNetControlMessageMetaAPI", tags);
    g_once_init_leave_pointer (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_net_control_message_meta_get_info (void)
{
  static const GstMetaInfo *meta_info = NULL;

  if (g_once_init_enter_pointer ((GstMetaInfo **) & meta_info)) {
    const GstMetaInfo *mi =
        gst_meta_register (GST_NET_CONTROL_MESSAGE_META_API_TYPE,
        "GstNetControlMessageMeta",
        sizeof (GstNetControlMessageMeta),
        net_control_message_meta_init,
        net_control_message_meta_free,
        net_control_message_meta_transform);
    g_once_init_leave_pointer ((GstMetaInfo **) & meta_info, (GstMetaInfo *) mi);
  }
  return meta_info;
}

/**
 * gst_buffer_add_net_control_message_meta:
 * @buffer: a #GstBuffer
 * @message: a @GSocketControlMessage to attach to @buffer
 *
 * Attaches @message as metadata in a #GstNetControlMessageMeta to @buffer.
 *
 * Returns: (transfer none): a #GstNetControlMessageMeta connected to @buffer
 */
GstNetControlMessageMeta *
gst_buffer_add_net_control_message_meta (GstBuffer * buffer,
    GSocketControlMessage * message)
{
  GstNetControlMessageMeta *meta;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);
  g_return_val_if_fail (G_IS_SOCKET_CONTROL_MESSAGE (message), NULL);

  meta =
      (GstNetControlMessageMeta *) gst_buffer_add_meta (buffer,
      GST_NET_CONTROL_MESSAGE_META_INFO, NULL);

  meta->message = g_object_ref (message);

  return meta;
}
