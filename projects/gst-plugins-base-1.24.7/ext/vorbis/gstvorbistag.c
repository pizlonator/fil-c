/*
 * Copyright (C) 2006 James Livingston <doclivingston@gmail.com>
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
 * SECTION:element-vorbistag
 * @title: vorbistag
 * @see_also: #oggdemux, #oggmux, #vorbisparse, #GstTagSetter
 *
 * The vorbistags element can change the tag contained within a raw
 * vorbis stream. Specifically, it modifies the comments header packet
 * of the vorbis stream.
 *
 * The element will also process the stream as the #vorbisparse element does
 * so it can be used when remuxing an Ogg Vorbis stream, without additional
 * elements.
 *
 * Applications can set the tags to write using the #GstTagSetter interface.
 * Tags contained within the vorbis bitstream will be picked up
 * automatically (and merged according to the merge mode set via the tag
 * setter interface).
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=foo.ogg ! oggdemux ! vorbistag ! oggmux ! filesink location=bar.ogg
 * ]|
 *  This element is not useful with gst-launch-1.0, because it does not support
 * setting the tags on a #GstTagSetter interface. Conceptually, the element
 * will usually be used in this order though.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gst/tag/tag.h>
#include <gst/gsttagsetter.h>

#include <vorbis/codec.h>

#include "gstvorbiselements.h"
#include "gstvorbistag.h"


GST_DEBUG_CATEGORY_STATIC (vorbistag_debug);
#define GST_CAT_DEFAULT vorbistag_debug

static GstFlowReturn gst_vorbis_tag_parse_packet (GstVorbisParse * parse,
    GstBuffer * buffer);

#define gst_vorbis_tag_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVorbisTag, gst_vorbis_tag,
    GST_TYPE_VORBIS_PARSE, G_IMPLEMENT_INTERFACE (GST_TYPE_TAG_SETTER, NULL));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vorbistag, "vorbistag",
    GST_RANK_NONE, GST_TYPE_VORBIS_TAG,
    GST_DEBUG_CATEGORY_INIT (vorbistag_debug, "vorbistag", 0,
        "vorbis tagging element"); vorbis_element_init (plugin));

static void
gst_vorbis_tag_class_init (GstVorbisTagClass * klass)
{
  GstVorbisParseClass *vorbisparse_class = GST_VORBIS_PARSE_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_static_metadata (element_class,
      "VorbisTag", "Formatter/Metadata",
      "Retags vorbis streams", "James Livingston <doclivingston@gmail.com>");

  vorbisparse_class->parse_packet = gst_vorbis_tag_parse_packet;
}

static void
gst_vorbis_tag_init (GstVorbisTag * tagger)
{
  /* nothing to do */
}


static GstFlowReturn
gst_vorbis_tag_parse_packet (GstVorbisParse * parse, GstBuffer * buffer)
{
  GstTagList *old_tags, *new_tags;
  const GstTagList *user_tags;
  GstVorbisTag *tagger;
  gchar *encoder = NULL;
  GstBuffer *new_buf;
  GstMapInfo map;
  gboolean do_parse = FALSE;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  /* just pass everything except the comments packet */
  if (map.size >= 1 && map.data[0] != 0x03)
    do_parse = TRUE;
  gst_buffer_unmap (buffer, &map);

  if (do_parse) {
    return GST_VORBIS_PARSE_CLASS (parent_class)->parse_packet (parse, buffer);
  }

  tagger = GST_VORBIS_TAG (parse);

  old_tags =
      gst_tag_list_from_vorbiscomment_buffer (buffer, (guint8 *) "\003vorbis",
      7, &encoder);
  user_tags = gst_tag_setter_get_tag_list (GST_TAG_SETTER (tagger));

  /* build new tag list */
  new_tags = gst_tag_list_merge (user_tags, old_tags,
      gst_tag_setter_get_tag_merge_mode (GST_TAG_SETTER (tagger)));
  gst_tag_list_unref (old_tags);

  new_buf =
      gst_tag_list_to_vorbiscomment_buffer (new_tags, (guint8 *) "\003vorbis",
      7, encoder);
  gst_buffer_copy_into (new_buf, buffer, GST_BUFFER_COPY_TIMESTAMPS, 0, -1);

  gst_tag_list_unref (new_tags);
  g_free (encoder);
  gst_buffer_unref (buffer);

  return GST_VORBIS_PARSE_CLASS (parent_class)->parse_packet (parse, new_buf);
}
