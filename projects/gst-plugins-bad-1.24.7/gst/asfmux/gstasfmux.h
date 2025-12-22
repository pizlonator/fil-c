/* ASF muxer plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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


#ifndef __GST_ASF_MUX_H__
#define __GST_ASF_MUX_H__


#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <gst/riff/riff-media.h>

#include "gstasfobjects.h"

G_BEGIN_DECLS
#define GST_TYPE_ASF_MUX \
  (gst_asf_mux_get_type())
#define GST_ASF_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ASF_MUX,GstAsfMux))
#define GST_ASF_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ASF_MUX,GstAsfMuxClass))
#define GST_IS_ASF_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ASF_MUX))
#define GST_IS_ASF_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ASF_MUX))
#define GST_ASF_MUX_CAST(obj) ((GstAsfMux*)(obj))
typedef struct _GstAsfMux GstAsfMux;
typedef struct _GstAsfMuxClass GstAsfMuxClass;
typedef struct _GstAsfPad GstAsfPad;
typedef struct _GstAsfAudioPad GstAsfAudioPad;
typedef struct _GstAsfVideoPad GstAsfVideoPad;
typedef enum _GstAsfMuxState GstAsfMuxState;

enum _GstAsfMuxState
{
  GST_ASF_MUX_STATE_NONE,
  GST_ASF_MUX_STATE_HEADERS,
  GST_ASF_MUX_STATE_DATA,
  GST_ASF_MUX_STATE_EOS
};

struct _GstAsfPad
{
  GstCollectData collect;

  gboolean is_audio;
  guint8 stream_number;
  guint8 media_object_number;
  guint32 bitrate;

  GstClockTime play_duration;
  GstClockTime first_ts;

  GstBuffer *codec_data;

  /* stream only metadata */
  GstTagList *taglist;
};

struct _GstAsfAudioPad
{
  GstAsfPad pad;

  gst_riff_strf_auds audioinfo;
};

struct _GstAsfVideoPad
{
  GstAsfPad pad;

  gst_riff_strf_vids vidinfo;

  /* Simple Index Entries */
  GSList *simple_index;
  gboolean has_keyframe;        /* if we have received one at least */
  guint32 last_keyframe_packet;
  guint16 last_keyframe_packet_count;
  guint16 max_keyframe_packet_count;
  GstClockTime next_index_time;
  guint64 time_interval;
};

struct _GstAsfMux
{
  GstElement element;

  /* output stream state */
  GstAsfMuxState state;

  /* counter to assign stream numbers */
  guint8 stream_number;

  /* counting variables */
  guint64 file_size;
  guint64 data_object_size;
  guint64 total_data_packets;

  /*
   * data object size field position
   * needed for updating when finishing the file
   */
  guint64 data_object_position;
  guint64 file_properties_object_position;

  /* payloads still to be sent in a packet */
  guint32 payload_data_size;
  guint32 payload_parsing_info_size;
  GSList *payloads;

  Guid file_id;

  /* properties */
  guint32 prop_packet_size;
  guint64 prop_preroll;
  gboolean prop_merge_stream_tags;
  guint64 prop_padding;
  gboolean prop_streamable;

  /* same as properties, but those are stored here to be
   * used without modification while muxing a single file */
  guint32 packet_size;
  guint64 preroll;              /* milliseconds */
  gboolean merge_stream_tags;

  GstClockTime first_ts;

  /* pads */
  GstPad *srcpad;

  GstCollectPads *collect;
};

struct _GstAsfMuxClass
{
  GstElementClass parent_class;
};

GType gst_asf_mux_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (asfmux);

G_END_DECLS
#endif /* __GST_ASF_MUX_H__ */
