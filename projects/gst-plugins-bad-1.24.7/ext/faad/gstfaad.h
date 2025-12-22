/* GStreamer FAAD (Free AAC Decoder) plugin
 * Copyright (C) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
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

#ifndef __GST_FAAD_H__
#define __GST_FAAD_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiodecoder.h>

#include <neaacdec.h>

G_BEGIN_DECLS

#define GST_TYPE_FAAD \
  (gst_faad_get_type ())
#define GST_FAAD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_FAAD, GstFaad))
#define GST_FAAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_FAAD, GstFaadClass))
#define GST_IS_FAAD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_FAAD))
#define GST_IS_FAAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_FAAD))

typedef struct _GstFaad {
  GstAudioDecoder element;

  guint      samplerate; /* sample rate of the last MPEG frame    */
  guint      channels;   /* number of channels of the last frame  */
  guint      bps;        /* bytes per sample                      */
  guchar    *channel_positions;
  GstAudioChannelPosition aac_positions[6], gst_positions[6];
  gboolean   need_reorder;
  gint       reorder_map[64];

  guint8     fake_codec_data[2];
  guint32    last_header;

  /* FAAD object */
  faacDecHandle handle;
  gboolean init;

  gboolean packetised; /* We must differentiate between raw and packetised streams */

} GstFaad;

typedef struct _GstFaadClass {
  GstAudioDecoderClass parent_class;
} GstFaadClass;

GType gst_faad_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (faad);

G_END_DECLS

#endif /* __GST_FAAD_H__ */
