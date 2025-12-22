/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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
# include "config.h"
#endif

#include <stdlib.h>

#include "utils.h"
#include "gstwebrtcbin.h"

GstPadTemplate *
_find_pad_template (GstElement * element, GstPadDirection direction,
    GstPadPresence presence, const gchar * name)
{
  GstElementClass *element_class = GST_ELEMENT_GET_CLASS (element);
  const GList *l = gst_element_class_get_pad_template_list (element_class);
  GstPadTemplate *templ = NULL;

  for (; l; l = l->next) {
    templ = l->data;
    if (templ->direction != direction)
      continue;
    if (templ->presence != presence)
      continue;
    if (g_strcmp0 (templ->name_template, name) == 0) {
      return templ;
    }
  }

  return NULL;
}

GstSDPMessage *
_get_latest_offer (GstWebRTCBin * webrtc)
{
  if (webrtc->current_local_description &&
      webrtc->current_local_description->type == GST_WEBRTC_SDP_TYPE_OFFER) {
    return webrtc->current_local_description->sdp;
  }
  if (webrtc->current_remote_description &&
      webrtc->current_remote_description->type == GST_WEBRTC_SDP_TYPE_OFFER) {
    return webrtc->current_remote_description->sdp;
  }

  return NULL;
}

GstSDPMessage *
_get_latest_answer (GstWebRTCBin * webrtc)
{
  if (webrtc->current_local_description &&
      webrtc->current_local_description->type == GST_WEBRTC_SDP_TYPE_ANSWER) {
    return webrtc->current_local_description->sdp;
  }
  if (webrtc->current_remote_description &&
      webrtc->current_remote_description->type == GST_WEBRTC_SDP_TYPE_ANSWER) {
    return webrtc->current_remote_description->sdp;
  }

  return NULL;
}

GstSDPMessage *
_get_latest_sdp (GstWebRTCBin * webrtc)
{
  GstSDPMessage *ret = NULL;

  if ((ret = _get_latest_answer (webrtc)))
    return ret;
  if ((ret = _get_latest_offer (webrtc)))
    return ret;

  return NULL;
}

GstSDPMessage *
_get_latest_self_generated_sdp (GstWebRTCBin * webrtc)
{
  if (webrtc->priv->last_generated_answer)
    return webrtc->priv->last_generated_answer->sdp;
  if (webrtc->priv->last_generated_offer)
    return webrtc->priv->last_generated_offer->sdp;

  return NULL;
}

struct pad_block *
_create_pad_block (GstElement * element, GstPad * pad, gulong block_id,
    gpointer user_data, GDestroyNotify notify)
{
  struct pad_block *ret = g_new0 (struct pad_block, 1);

  ret->element = gst_object_ref (element);
  ret->pad = gst_object_ref (pad);
  ret->block_id = block_id;
  ret->user_data = user_data;
  ret->notify = notify;

  return ret;
}

void
_free_pad_block (struct pad_block *block)
{
  if (!block)
    return;

  if (block->block_id)
    gst_pad_remove_probe (block->pad, block->block_id);
  gst_object_unref (block->element);
  gst_object_unref (block->pad);
  if (block->notify)
    block->notify (block->user_data);
  g_free (block);
}

const gchar *
_enum_value_to_string (GType type, guint value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  const gchar *str = NULL;

  enum_class = g_type_class_ref (type);
  enum_value = g_enum_get_value (enum_class, value);

  if (enum_value)
    str = enum_value->value_nick;

  g_type_class_unref (enum_class);

  return str;
}

const gchar *
_g_checksum_to_webrtc_string (GChecksumType type)
{
  switch (type) {
    case G_CHECKSUM_SHA1:
      return "sha-1";
    case G_CHECKSUM_SHA256:
      return "sha-256";
#ifdef G_CHECKSUM_SHA384
    case G_CHECKSUM_SHA384:
      return "sha-384";
#endif
    case G_CHECKSUM_SHA512:
      return "sha-512";
    default:
      g_warning ("unknown GChecksumType!");
      return NULL;
  }
}

void
_remove_optional_offer_fields (GstCaps * offer_caps)
{
  int i;

  for (i = 0; i < gst_caps_get_size (offer_caps); i++) {
    GstStructure *s = gst_caps_get_structure (offer_caps, i);
    const gchar *mtype = gst_structure_get_string (s, "media");
    const gchar *encoding_name = gst_structure_get_string (s, "encoding-name");

    if (mtype == NULL || encoding_name == NULL) {
      continue;
    }

    /* Special cases for different codecs - sender-only fields
     * that we don't need to care about for SDP intersection */
    if (g_str_equal (mtype, "audio")) {
      if (g_str_equal (encoding_name, "OPUS")) {
        gst_structure_remove_fields (s, "sprop-stereo", "sprop-maxcapturerate",
            NULL);
      }
    }
  }
}

GstCaps *
_rtp_caps_from_media (const GstSDPMedia * media)
{
  GstCaps *ret;
  int i, j;

  ret = gst_caps_new_empty ();
  for (i = 0; i < gst_sdp_media_formats_len (media); i++) {
    guint pt = atoi (gst_sdp_media_get_format (media, i));
    GstCaps *caps;

    caps = gst_sdp_media_get_caps_from_media (media, pt);
    if (!caps)
      continue;

    /* gst_sdp_media_get_caps_from_media() produces caps with name
     * "application/x-unknown" which will fail intersection with
     * "application/x-rtp" caps so mangle the returns caps to have the
     * correct name here */
    for (j = 0; j < gst_caps_get_size (caps); j++) {
      GstStructure *s = gst_caps_get_structure (caps, j);
      gst_structure_set_name (s, "application/x-rtp");
    }

    gst_caps_append (ret, caps);
  }

  return ret;
}

GstWebRTCKind
webrtc_kind_from_caps (const GstCaps * caps)
{
  GstStructure *s;
  const gchar *media;

  if (!caps || gst_caps_get_size (caps) == 0)
    return GST_WEBRTC_KIND_UNKNOWN;

  s = gst_caps_get_structure (caps, 0);

  media = gst_structure_get_string (s, "media");
  if (media == NULL)
    return GST_WEBRTC_KIND_UNKNOWN;

  if (!g_strcmp0 (media, "audio"))
    return GST_WEBRTC_KIND_AUDIO;

  if (!g_strcmp0 (media, "video"))
    return GST_WEBRTC_KIND_VIDEO;

  return GST_WEBRTC_KIND_UNKNOWN;
}

char *
_get_msid_from_media (const GstSDPMedia * media)
{
  int i;

  for (i = 0; i < gst_sdp_media_attributes_len (media); i++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, i);
    const char *start, *end;

    if (!attr->value)
      continue;

    start = strstr (attr->value, "msid:");
    if (!start)
      continue;

    start += strlen ("msid:");
    end = strstr (start, " ");
    if (end)
      return g_strndup (start, end - start);
  }

  return NULL;
}
