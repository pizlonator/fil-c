/* GStreamer
 * Copyright (C) 2018 Thibault Saunier <tsaunier@igalia.com>
 *
 * alsadeviceprovider.c: alsa device probing and monitoring
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstalsadeviceprovider.h"
#include <string.h>
#include <gst/gst.h>


static GstDevice *gst_alsa_device_new (const gchar * device_name,
    GstCaps * caps, const gchar * internal_name, snd_pcm_stream_t stream,
    GstStructure * properties);

G_DEFINE_TYPE (GstAlsaDeviceProvider, gst_alsa_device_provider,
    GST_TYPE_DEVICE_PROVIDER);

static GstStaticCaps alsa_caps = GST_STATIC_CAPS ("audio/x-raw, "
    "format = (string) " GST_AUDIO_FORMATS_ALL ", "
    "layout = (string) interleaved, "
    "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
    PASSTHROUGH_CAPS);

static GstDevice *
add_device (GstDeviceProvider * provider, snd_ctl_t * info,
    snd_pcm_stream_t stream, gint card, gint dev)
{
  GstCaps *caps, *template;
  GstDevice *device;
  snd_pcm_t *handle;
  snd_ctl_card_info_t *card_info;
  GstStructure *props;
  gchar *card_name, *longname = NULL;
  gchar *device_name = g_strdup_printf ("hw:%d,%d", card, dev);

  if (snd_pcm_open (&handle, device_name, stream, SND_PCM_NONBLOCK) < 0) {
    GST_ERROR_OBJECT (provider, "Could not open device %s for inspection!",
        device_name);
    g_free (device_name);

    return NULL;
  }

  template = gst_static_caps_get (&alsa_caps);
  caps = gst_alsa_probe_supported_formats (GST_OBJECT (provider),
      device_name, handle, template);
  gst_caps_unref (template);

  snd_card_get_name (card, &card_name);
  props = gst_structure_new ("alsa-proplist",
      "device.api", G_TYPE_STRING, "alsa",
      "device.class", G_TYPE_STRING, "sound",
      "alsa.card", G_TYPE_INT, card,
      "alsa.card_name", G_TYPE_STRING, card_name, NULL);
  g_free (card_name);

  snd_ctl_card_info_alloca (&card_info);
  if (snd_ctl_card_info (info, card_info) == 0) {
    gst_structure_set (props,
        "alsa.driver_name", G_TYPE_STRING,
        snd_ctl_card_info_get_driver (card_info), "alsa.name", G_TYPE_STRING,
        snd_ctl_card_info_get_name (card_info), "alsa.id", G_TYPE_STRING,
        snd_ctl_card_info_get_id (card_info), "alsa.mixername", G_TYPE_STRING,
        snd_ctl_card_info_get_mixername (card_info), "alsa.components",
        G_TYPE_STRING, snd_ctl_card_info_get_components (card_info), NULL);

    snd_ctl_card_info_clear (card_info);
  }

  snd_card_get_longname (card, &longname);
  device = gst_alsa_device_new (longname, caps, device_name, stream, props);

  snd_pcm_close (handle);

  return device;
}

static GList *
gst_alsa_device_provider_probe (GstDeviceProvider * provider)
{
  snd_ctl_t *handle;
  int card, dev;
  snd_ctl_card_info_t *info;
  snd_pcm_info_t *pcminfo;
  GList *list = NULL;
  gint i;
  gint streams[] = { SND_PCM_STREAM_CAPTURE, SND_PCM_STREAM_PLAYBACK };
  snd_pcm_stream_t stream;

  GST_INFO_OBJECT (provider, "Probing alsa devices");
  snd_ctl_card_info_malloc (&info);
  snd_pcm_info_malloc (&pcminfo);

  for (i = 0; i < G_N_ELEMENTS (streams); i++) {
    card = -1;
    stream = streams[i];

    if (snd_card_next (&card) < 0 || card < 0) {
      /* no soundcard found */
      GST_WARNING_OBJECT (provider, "No soundcard found");
      goto beach;
    }

    while (card >= 0) {
      gchar name[32];

      g_snprintf (name, sizeof (name), "hw:%d", card);
      if (snd_ctl_open (&handle, name, 0) < 0)
        goto next_card;

      if (snd_ctl_card_info (handle, info) < 0) {
        snd_ctl_close (handle);
        goto next_card;
      }

      dev = -1;
      while (1) {
        GstDevice *device;
        snd_ctl_pcm_next_device (handle, &dev);

        if (dev < 0)
          break;
        snd_pcm_info_set_device (pcminfo, dev);
        snd_pcm_info_set_subdevice (pcminfo, 0);
        snd_pcm_info_set_stream (pcminfo, stream);
        if (snd_ctl_pcm_info (handle, pcminfo) < 0) {
          continue;
        }

        device = add_device (provider, handle, stream, card, dev);
        if (device)
          list = g_list_prepend (list, device);
      }
      snd_ctl_close (handle);
    next_card:
      if (snd_card_next (&card) < 0) {
        break;
      }
    }
  }

beach:
  snd_ctl_card_info_free (info);
  snd_pcm_info_free (pcminfo);

  return list;
}


enum
{
  PROP_0,
  PROP_LAST
};


static void
gst_alsa_device_provider_class_init (GstAlsaDeviceProviderClass * klass)
{
  GstDeviceProviderClass *dm_class = GST_DEVICE_PROVIDER_CLASS (klass);

  dm_class->probe = gst_alsa_device_provider_probe;

  gst_device_provider_class_set_static_metadata (dm_class,
      "ALSA Device Provider", "Sink/Source/Audio",
      "List and provides Alsa source and sink devices",
      "Thibault Saunier <tsaunier@igalia.com>");
}

static void
gst_alsa_device_provider_init (GstAlsaDeviceProvider * self)
{
}

/*** GstAlsaDevice implementation ******/
enum
{
  PROP_INTERNAL_NAME = 1,
};


G_DEFINE_TYPE (GstAlsaDevice, gst_alsa_device, GST_TYPE_DEVICE);

static GstElement *
gst_alsa_device_create_element (GstDevice * device, const gchar * name)
{
  GstAlsaDevice *alsa_dev = GST_ALSA_DEVICE (device);
  GstElement *elem;

  elem = gst_element_factory_make (alsa_dev->element, name);
  g_object_set (elem, "device", alsa_dev->internal_name, NULL);

  return elem;
}

static gboolean
gst_alsa_device_reconfigure_element (GstDevice * device, GstElement * element)
{
  GstAlsaDevice *alsa_dev = GST_ALSA_DEVICE (device);

  g_object_set (element, "device", alsa_dev->internal_name, NULL);

  return TRUE;
}


static void
gst_alsa_device_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAlsaDevice *device;

  device = GST_ALSA_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_INTERNAL_NAME:
      g_value_set_string (value, device->internal_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_alsa_device_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAlsaDevice *device;

  device = GST_ALSA_DEVICE_CAST (object);

  switch (prop_id) {
    case PROP_INTERNAL_NAME:
      device->internal_name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_alsa_device_finalize (GObject * object)
{
  GstAlsaDevice *device = GST_ALSA_DEVICE (object);

  g_free (device->internal_name);

  G_OBJECT_CLASS (gst_alsa_device_parent_class)->finalize (object);
}

static void
gst_alsa_device_class_init (GstAlsaDeviceClass * klass)
{
  GstDeviceClass *dev_class = GST_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  dev_class->create_element = gst_alsa_device_create_element;
  dev_class->reconfigure_element = gst_alsa_device_reconfigure_element;

  object_class->get_property = gst_alsa_device_get_property;
  object_class->set_property = gst_alsa_device_set_property;
  object_class->finalize = gst_alsa_device_finalize;

  g_object_class_install_property (object_class, PROP_INTERNAL_NAME,
      g_param_spec_string ("internal-name", "Internal AlsaAudio device name",
          "The internal name of the AlsaAudio device", "",
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gst_alsa_device_init (GstAlsaDevice * device)
{
}

/* Takes ownership of @caps and @props */
static GstDevice *
gst_alsa_device_new (const gchar * device_name,
    GstCaps * caps, const gchar * internal_name,
    snd_pcm_stream_t stream, GstStructure * props)
{
  GstAlsaDevice *gstdev;
  const gchar *element = NULL;
  const gchar *klass = NULL;

  g_return_val_if_fail (device_name, NULL);
  g_return_val_if_fail (internal_name, NULL);
  g_return_val_if_fail (caps, NULL);

  switch (stream) {
    case SND_PCM_STREAM_CAPTURE:
      element = "alsasrc";
      klass = "Audio/Source";
      break;
    case SND_PCM_STREAM_PLAYBACK:
      element = "alsasink";
      klass = "Audio/Sink";
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  gstdev = g_object_new (GST_TYPE_ALSA_DEVICE,
      "display-name", device_name, "caps", caps, "device-class", klass,
      "internal-name", internal_name, "properties", props, NULL);

  gstdev->stream = stream;
  gstdev->element = element;

  gst_structure_free (props);
  gst_caps_unref (caps);

  return GST_DEVICE (gstdev);
}
