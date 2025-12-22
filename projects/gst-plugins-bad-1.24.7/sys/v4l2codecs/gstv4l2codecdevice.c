/* GStreamer
 * Copyright (C) 2020 Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstv4l2codecdevice.h"
#include "linux/media.h"

#include <fcntl.h>
#include <gudev/gudev.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#if HAVE_MAKEDEV_IN_MKDEV
#include <sys/mkdev.h>
#elif HAVE_MAKEDEV_IN_SYSMACROS
#include <sys/sysmacros.h>
#elif HAVE_MAKEDEV_IN_TYPES
#include <sys/types.h>
#endif
#include <unistd.h>

#define GST_CAT_DEFAULT gstv4l2codecs_debug
GST_DEBUG_CATEGORY_EXTERN (gstv4l2codecs_debug);

GST_DEFINE_MINI_OBJECT_TYPE (GstV4l2CodecDevice, gst_v4l2_codec_device);

static void
gst_v4l2_codec_device_free (GstV4l2CodecDevice * device)
{
  g_free (device->name);
  g_free (device->media_device_path);
  g_free (device->video_device_path);
  g_free (device);
}

static GstV4l2CodecDevice *
gst_v4l2_codec_device_new (const gchar * name, guint32 function,
    const gchar * media_device_path, const gchar * video_device_path)
{
  GstV4l2CodecDevice *device = g_new0 (GstV4l2CodecDevice, 1);

  gst_mini_object_init (GST_MINI_OBJECT_CAST (device),
      0, GST_TYPE_V4L2_CODEC_DEVICE, NULL, NULL,
      (GstMiniObjectFreeFunction) gst_v4l2_codec_device_free);

  device->name = g_strdup (name);
  device->function = function;
  device->media_device_path = g_strdup (media_device_path);
  device->video_device_path = g_strdup (video_device_path);

  return device;
}

static void
clear_topology (struct media_v2_topology *topology)
{
  g_free ((gpointer) (gsize) topology->ptr_entities);
  g_free ((gpointer) (gsize) topology->ptr_interfaces);
  g_free ((gpointer) (gsize) topology->ptr_pads);
  g_free ((gpointer) (gsize) topology->ptr_links);
  memset (topology, 0, sizeof (struct media_v2_topology));
}

static gboolean
get_topology (gint fd, struct media_v2_topology *topology)
{
  gint ret;
  guint64 version;

again:
  memset (topology, 0, sizeof (struct media_v2_topology));

  ret = ioctl (fd, MEDIA_IOC_G_TOPOLOGY, topology);
  if (ret < 0) {
    GST_WARNING ("Could not retrieve topology: %s", g_strerror (errno));
    return FALSE;
  }

  version = topology->topology_version;
  topology->ptr_entities = (guint64) (gsize)
      g_new0 (struct media_v2_entity, topology->num_entities);
  topology->ptr_interfaces = (guint64) (gsize)
      g_new0 (struct media_v2_interface, topology->num_interfaces);
  topology->ptr_pads = (guint64) (gsize)
      g_new0 (struct media_v2_pad, topology->num_pads);
  topology->ptr_links = (guint64) (gsize)
      g_new0 (struct media_v2_link, topology->num_links);

  ret = ioctl (fd, MEDIA_IOC_G_TOPOLOGY, topology);
  if (ret < 0) {
    GST_WARNING ("Could not retrieve topology: %s", g_strerror (errno));
    clear_topology (topology);
    return FALSE;
  }

  /* If the topology have changed, just retry */
  if (version != topology->topology_version) {
    clear_topology (topology);
    goto again;
  }

  return TRUE;
}

static struct media_v2_entity *
find_v4l_entity (struct media_v2_topology *topology, guint32 id)
{
  gint i;

  for (i = 0; i < topology->num_entities; i++) {
    struct media_v2_entity *entity =
        ((struct media_v2_entity *) (gsize) topology->ptr_entities) + i;
    if (entity->function != MEDIA_ENT_F_IO_V4L)
      continue;
    if (entity->id == id)
      return entity;
  }

  return NULL;
}

static struct media_v2_pad *
find_pad (struct media_v2_topology *topology, guint32 id)
{
  gint i;

  for (i = 0; i < topology->num_pads; i++) {
    struct media_v2_pad *pad =
        ((struct media_v2_pad *) (gsize) topology->ptr_pads) + i;
    if (pad->id == id)
      return pad;
  }

  return NULL;
}

static GList *
find_codec_entity (struct media_v2_topology *topology)
{
  GQueue entities = G_QUEUE_INIT;
  gint i;

  for (i = 0; i < topology->num_entities; i++) {
    struct media_v2_entity *entity =
        ((struct media_v2_entity *) (gsize) topology->ptr_entities) + i;

    switch (entity->function) {
      case MEDIA_ENT_F_PROC_VIDEO_ENCODER:
      case MEDIA_ENT_F_PROC_VIDEO_DECODER:
        g_queue_push_tail (&entities, entity);
        break;
      default:
        break;
    }
  }

  return entities.head;
}

static gboolean
find_codec_entity_pads (struct media_v2_topology *topology,
    struct media_v2_entity *entity, struct media_v2_pad **sink_pad,
    struct media_v2_pad **source_pad)
{
  gint i;

  *sink_pad = NULL;
  *source_pad = NULL;

  for (i = 0; i < topology->num_pads; i++) {
    struct media_v2_pad *pad =
        ((struct media_v2_pad *) (gsize) topology->ptr_pads) + i;

    if (pad->entity_id != entity->id)
      continue;

    if (pad->flags & MEDIA_PAD_FL_SINK) {
      if (*sink_pad)
        return FALSE;
      *sink_pad = pad;
    } else if (pad->flags & MEDIA_PAD_FL_SOURCE) {
      if (*source_pad)
        return FALSE;
      *source_pad = pad;
    } else {
      /* unknown pad type */
      return FALSE;
    }
  }

  return (*source_pad && *sink_pad);
}

static struct media_v2_entity *
find_peer_v4l_entity (struct media_v2_topology *topology,
    struct media_v2_pad *pad)
{
  struct media_v2_pad *peer_pad = NULL;
  gint i;

  for (i = 0; i < topology->num_links; i++) {
    struct media_v2_link *link =
        ((struct media_v2_link *) (gsize) topology->ptr_links) + i;

    if ((link->flags & MEDIA_LNK_FL_LINK_TYPE) != MEDIA_LNK_FL_DATA_LINK)
      continue;

    if ((link->flags & (MEDIA_LNK_FL_IMMUTABLE)) == 0)
      continue;

    if ((link->flags & (MEDIA_LNK_FL_ENABLED)) == 0)
      continue;

    if (pad->flags & MEDIA_PAD_FL_SINK && link->sink_id == pad->id)
      peer_pad = find_pad (topology, link->source_id);
    else if (pad->flags & MEDIA_PAD_FL_SOURCE && link->source_id == pad->id)
      peer_pad = find_pad (topology, link->sink_id);

    if (peer_pad)
      break;
  }

  if (peer_pad)
    return find_v4l_entity (topology, peer_pad->entity_id);

  return NULL;
}


static struct media_v2_interface *
find_video_interface (struct media_v2_topology *topology, guint32 id)
{
  gint i;

  for (i = 0; i < topology->num_interfaces; i++) {
    struct media_v2_interface *intf =
        ((struct media_v2_interface *) (gsize) topology->ptr_interfaces) + i;

    if (intf->intf_type != MEDIA_INTF_T_V4L_VIDEO)
      continue;

    if (intf->id == id)
      return intf;
  }

  return NULL;
}

static struct media_v2_intf_devnode *
find_video_devnode (struct media_v2_topology *topology,
    struct media_v2_entity *entity)
{
  gint i;

  for (i = 0; i < topology->num_links; i++) {
    struct media_v2_link *link =
        ((struct media_v2_link *) (gsize) topology->ptr_links) + i;
    struct media_v2_interface *intf;

    if ((link->flags & MEDIA_LNK_FL_LINK_TYPE) != MEDIA_LNK_FL_INTERFACE_LINK)
      continue;

    if (link->sink_id != entity->id)
      continue;

    intf = find_video_interface (topology, link->source_id);
    if (intf)
      return &intf->devnode;
  }

  return NULL;
}

static inline const gchar *
function_to_string (guint32 function)
{
  switch (function) {
    case MEDIA_ENT_F_PROC_VIDEO_ENCODER:
      return "encoder";
    case MEDIA_ENT_F_PROC_VIDEO_DECODER:
      return "decoder";
    default:
      break;
  }

  return "unknown";
}

GList *
gst_v4l2_codec_find_devices (void)
{
  GUdevClient *client;
  GList *udev_devices, *d;
  GQueue devices = G_QUEUE_INIT;

  client = g_udev_client_new (NULL);
  udev_devices = g_udev_client_query_by_subsystem (client, "media");

  if (!udev_devices)
    GST_DEBUG ("Found no media devices");

  for (d = udev_devices; d; d = g_list_next (d)) {
    GUdevDevice *udev = (GUdevDevice *) d->data;
    const gchar *path = g_udev_device_get_device_file (udev);
    gint fd;
    struct media_v2_topology topology;
    GList *codec_entities, *c;
    gboolean ret;

    fd = open (path, 0);
    if (fd < 0)
      continue;

    GST_DEBUG ("Analysing media device '%s'", path);

    ret = get_topology (fd, &topology);
    close (fd);

    if (!ret) {
      clear_topology (&topology);
      continue;
    }

    codec_entities = find_codec_entity (&topology);
    if (!codec_entities) {
      clear_topology (&topology);
      continue;
    }

    GST_DEBUG ("Found CODEC entities");

    for (c = codec_entities; c; c = g_list_next (c)) {
      struct media_v2_entity *entity = (struct media_v2_entity *) c->data;
      struct media_v2_pad *source_pad;
      struct media_v2_pad *sink_pad;
      struct media_v2_entity *source_entity, *sink_entity;
      struct media_v2_intf_devnode *source_dev, *sink_dev;
      GUdevDevice *v4l2_udev;

      GST_DEBUG ("Analysing entity %s", entity->name);

      if (!find_codec_entity_pads (&topology, entity, &sink_pad, &source_pad))
        continue;

      GST_DEBUG ("Found source and sink pads");

      source_entity = find_peer_v4l_entity (&topology, sink_pad);
      sink_entity = find_peer_v4l_entity (&topology, source_pad);

      if (!source_entity || !sink_entity)
        continue;

      GST_DEBUG ("Found source and sink V4L IO entities");

      source_dev = find_video_devnode (&topology, source_entity);
      sink_dev = find_video_devnode (&topology, source_entity);

      if (!source_dev || !sink_dev)
        continue;

      if (source_dev->major != sink_dev->major
          || source_dev->minor != sink_dev->minor)
        continue;

      v4l2_udev = g_udev_client_query_by_device_number (client,
          G_UDEV_DEVICE_TYPE_CHAR,
          makedev (source_dev->major, source_dev->minor));
      if (!v4l2_udev)
        continue;

      GST_INFO ("Found %s device %s", function_to_string (entity->function),
          entity->name);
      g_queue_push_tail (&devices,
          gst_v4l2_codec_device_new (entity->name, entity->function, path,
              g_udev_device_get_device_file (v4l2_udev)));
      g_object_unref (v4l2_udev);
    }

    clear_topology (&topology);
    g_list_free (codec_entities);
  }

  g_list_free_full (udev_devices, g_object_unref);
  g_object_unref (client);

  return devices.head;
}

void
gst_v4l2_codec_device_list_free (GList * devices)
{
  g_list_free_full (devices, (GDestroyNotify) gst_mini_object_unref);
}
