/* GStreamer
 * Copyright (C) 2010 Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 * Copyright (C) 2010 Nokia Corporation. All rights reserved.
 *   Contact: Stefan Kost <stefan.kost@nokia.com>
 *
 * Tremor modifications <2006>:
 *   Chris Lord, OpenedHand Ltd. <chris@openedhand.com>, http://www.o-hand.com/
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

#ifndef __GST_VORBIS_DEC_LIB_H__
#define __GST_VORBIS_DEC_LIB_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

#ifndef TREMOR

#define GST_VORBIS_DEC_DESCRIPTION "decode raw vorbis streams to float audio"

#define GST_VORBIS_AUDIO_FORMAT     GST_AUDIO_FORMAT_F32
#define GST_VORBIS_AUDIO_FORMAT_STR GST_AUDIO_NE (F32)

#define GST_VORBIS_DEC_SRC_CAPS       \
    GST_STATIC_CAPS ("audio/x-raw, "  \
        "format = (string)" GST_VORBIS_AUDIO_FORMAT_STR ", "     \
        "rate = (int) [ 1, MAX ], "   \
        "channels = (int) [ 1, 256 ]")

#define GST_VORBIS_DEC_DEFAULT_SAMPLE_WIDTH           (32)

#else /* TREMOR */

#define GST_VORBIS_DEC_DESCRIPTION "decode raw vorbis streams to integer audio"

#define GST_VORBIS_AUDIO_FORMAT GST_AUDIO_FORMAT_S16
#define GST_VORBIS_AUDIO_FORMAT_STR GST_AUDIO_NE (S16)

#define GST_VORBIS_DEC_SRC_CAPS        \
    GST_STATIC_CAPS ("audio/x-raw, "   \
        "format = (string) " GST_VORBIS_AUDIO_FORMAT_STR ", "      \
        "rate = (int) [ 1, MAX ], "    \
        "channels = (int) [ 1, 6 ]")

#define GST_VORBIS_DEC_DEFAULT_SAMPLE_WIDTH           (16)

/* we need a different type name here */
#define GstVorbisDec GstIVorbisDec
#define GstVorbisDecClass GstIVorbisDecClass
#define gst_vorbis_dec_get_type gst_ivorbis_dec_get_type
#define gst_vorbis_get_copy_sample_func gst_ivorbis_get_copy_sample_func

#endif /* TREMOR */

#ifndef USE_TREMOLO

#ifdef TREMOR
 #include <tremor/ivorbiscodec.h>
 typedef ogg_int32_t                    vorbis_sample_t;
#else
 #include <vorbis/codec.h>
 typedef float                          vorbis_sample_t;
#endif

typedef ogg_packet                     ogg_packet_wrapper;

static inline guint8 *
gst_ogg_packet_data (ogg_packet * p)
{
  return (guint8 *) p->packet;
}

static inline gint
gst_ogg_packet_size (ogg_packet * p)
{
  return p->bytes;
}

static inline void
gst_ogg_packet_wrapper_map (ogg_packet * packet, GstBuffer * buffer, GstMapInfo *map)
{
  gst_buffer_ref (buffer);
  gst_buffer_map (buffer, map, GST_MAP_READ);
  packet->packet = map->data;
  packet->bytes = map->size;
}

static inline void
gst_ogg_packet_wrapper_unmap (ogg_packet * packet, GstBuffer * buffer, GstMapInfo *map)
{
  gst_buffer_unmap (buffer, map);
  gst_buffer_unref (buffer);
}

static inline ogg_packet *
gst_ogg_packet_from_wrapper (ogg_packet_wrapper * packet)
{
  return packet;
}

#else /* USE_TREMOLO */

#include <Tremolo/ivorbiscodec.h>
#include <Tremolo/codec_internal.h>
typedef ogg_int16_t                    vorbis_sample_t;
typedef struct _ogg_packet_wrapper     ogg_packet_wrapper;

struct _ogg_packet_wrapper {
  ogg_packet          packet;
  ogg_reference       ref;
  ogg_buffer          buf;
};

/* compensate minor variation */
#define vorbis_synthesis(a, b)             vorbis_synthesis (a, b, 1)

static inline guint8 *
gst_ogg_packet_data (ogg_packet * p)
{
  return (guint8 *) p->packet->buffer->data;
}

static inline gint
gst_ogg_packet_size (ogg_packet * p)
{
  return p->packet->buffer->size;
}

static inline void
gst_ogg_packet_wrapper_map (ogg_packet_wrapper * packet,
    GstBuffer * buffer, GstMapInfo * map)
{
  GstMapInfo info;
  ogg_reference *ref = &packet->ref;
  ogg_buffer *buf = &packet->buf;
  gsize size;

  gst_buffer_ref (buffer);
  gst_buffer_map (buffer, map, GST_MAP_READ);
  buf->data = map->data;
  buf->size = map->size;
  buf->refcount = 1;
  buf->ptr.owner = NULL;
  buf->ptr.next = NULL;

  ref->buffer = buf;
  ref->begin = 0;
  ref->length = buf->size;
  ref->next = NULL;

  packet->packet.packet = ref;
  packet->packet.bytes = ref->length;
}

static inline void
gst_ogg_packet_wrapper_unmap (ogg_packet_wrapper * packet,
    GstBuffer * buffer, GstMapInfo * map)
{
  ogg_reference *ref = &packet->ref;
  ogg_buffer *buf = &packet->buf;

  gst_buffer_unmap (buffer, map);
  gst_buffer_unref (buffer);
}

static inline ogg_packet *
gst_ogg_packet_from_wrapper (ogg_packet_wrapper * packet)
{
  return &(packet->packet);
}

#endif /* USE_TREMOLO */

typedef void (*CopySampleFunc)(vorbis_sample_t *out, vorbis_sample_t **in,
                           guint samples, gint channels);

CopySampleFunc gst_vorbis_get_copy_sample_func (gint channels);

#endif /* __GST_VORBIS_DEC_LIB_H__ */
