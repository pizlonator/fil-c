/* GStreamer
 * Copyright (C) 2014  Antonio Ospite <ao2@ao2.it>
 *
 * gstalsamidisrc.h: Source element for ALSA MIDI sequencer events
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

#ifndef __GST_ALSA_MIDI_SRC_H__
#define __GST_ALSA_MIDI_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <alsa/asoundlib.h>

G_BEGIN_DECLS
#define GST_TYPE_ALSA_MIDI_SRC \
  (gst_alsa_midi_src_get_type())
#define GST_ALSA_MIDI_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ALSA_MIDI_SRC,GstAlsaMidiSrc))
#define GST_ALSA_MIDI_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ALSA_MIDI_SRC,GstAlsaMidiSrcClass))
#define GST_IS_ALSA_MIDI_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ALSA_MIDI_SRC))
#define GST_IS_ALSA_MIDI_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ALSA_MIDI_SRC))

typedef struct _GstAlsaMidiSrc GstAlsaMidiSrc;
typedef struct _GstAlsaMidiSrcClass GstAlsaMidiSrcClass;

/**
 * GstAlsaMidiSrc:
 *
 * Opaque #GstAlsaMidiSrc data structure.
 */
struct _GstAlsaMidiSrc
{
  GstPushSrc element;

  gchar *ports;

  /*< private > */
  snd_seq_t *seq;
  int queue;
  int port_count;
  snd_seq_addr_t *seq_ports;
  snd_midi_event_t *parser;
  unsigned char *buffer;

  GstPoll *poll;

  guint64 tick;
  guint64 delay;
};

struct _GstAlsaMidiSrcClass
{
  GstPushSrcClass parent_class;
};

G_GNUC_INTERNAL GType gst_alsa_midi_src_get_type (void);

G_END_DECLS
#endif /* __GST_ALSA_MIDI_SRC_H__ */
