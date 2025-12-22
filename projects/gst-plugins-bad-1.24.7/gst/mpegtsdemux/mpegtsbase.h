/*
 * mpegtsbase.h - GStreamer MPEG transport stream base class
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *               2007 Alessandro Decina
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *  Author: Youness Alaoui <youness.alaoui@collabora.co.uk>, Collabora Ltd.
 *  Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
 *   Edward Hervey <edward.hervey@collabora.co.uk>
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


#ifndef GST_MPEG_TS_BASE_H
#define GST_MPEG_TS_BASE_H

#include <gst/gst.h>
#include "mpegtspacketizer.h"

G_BEGIN_DECLS

#define GST_TYPE_MPEGTS_BASE \
  (mpegts_base_get_type())
#define GST_MPEGTS_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPEGTS_BASE,MpegTSBase))
#define GST_MPEGTS_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPEGTS_BASE,MpegTSBaseClass))
#define GST_IS_MPEGTS_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPEGTS_BASE))
#define GST_IS_MPEGTS_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPEGTS_BASE))
#define GST_MPEGTS_BASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MPEGTS_BASE, MpegTSBaseClass))

#define MPEG_TS_BASE_PACKETIZER(b) (((MpegTSBase*)b)->packetizer)

typedef struct _MpegTSBase MpegTSBase;
typedef struct _MpegTSBaseClass MpegTSBaseClass;
typedef struct _MpegTSBaseStream MpegTSBaseStream;
typedef struct _MpegTSBaseProgram MpegTSBaseProgram;

struct _MpegTSBaseStream
{
  guint16             pid;
  guint8              stream_type;

  /* Content of the registration descriptor (if present) */
  guint32             registration_id;

  GstMpegtsPMTStream *stream;
  GstStream          *stream_object;
  gboolean            in_collection;
  gchar              *stream_id;
};

struct _MpegTSBaseProgram
{
  gint                program_number;
  guint16             pmt_pid;
  guint16             pcr_pid;

  /* Content of the registration descriptor (if present) */
  guint32             registration_id;

  GstMpegtsSection   *section;
  const GstMpegtsPMT *pmt;

  MpegTSBaseStream  **streams;
  GList              *stream_list;
  gint                patcount;

  GstStreamCollection *collection;

  /* Pending Tags for the program */
  GstTagList *tags;
  guint event_id;

  /* TRUE if the program is currently being used */
  gboolean active;
  /* TRUE if this is the first program created */
  gboolean initial_program;

  /* TRUE if the program shouldn't be freed */
  gboolean recycle;
};

typedef enum {
  /* PULL MODE */
  BASE_MODE_SCANNING,		/* Looking for PAT/PMT */
  BASE_MODE_SEEKING,		/* Seeking */
  BASE_MODE_STREAMING,		/* Normal mode (pushing out data) */

  /* PUSH MODE */
  BASE_MODE_PUSHING
} MpegTSBaseMode;

struct _MpegTSBase {
  GstElement element;

  GstPad *sinkpad;

  /* pull-based behaviour */
  MpegTSBaseMode mode;

  /* Current pull offset (also set by seek handler) */
  guint64	seek_offset;

  /* Cached packetsize */
  guint16	packetsize;

  /* the following vars must be protected with the OBJECT_LOCK as they can be
   * accessed from the application thread and the streaming thread */
  GPtrArray *programs;

  GPtrArray  *pat;
  MpegTSPacketizer2 *packetizer;

  /* arrays that say whether a pid is a known psi pid or a pes pid */
  /* Use MPEGTS_BIT_* to set/unset/check the values */
  guint8 *known_psi;
  guint8 *is_pes;

  gboolean disposed;

  /* size of the MpegTSBaseProgram structure, can be overridden
   * by subclasses if they have their own MpegTSBaseProgram subclasses. */
  gsize program_size;

  /* size of the MpegTSBaseStream structure, can be overridden
   * by subclasses if they have their own MpegTSBaseStream subclasses */
  gsize stream_size;

  /* Whether we saw a PAT yet */
  gboolean seen_pat;

  /* Upstream segment */
  GstSegment segment;

  /* Downstream segment, for use by sub-classes */
  GstSegment out_segment;

  /* Last received seek event seqnum (default GST_SEQNUM_INVALID) */
  guint last_seek_seqnum;

  /* Whether to parse private section or not */
  gboolean parse_private_sections;

  /* Whether to push data and/or sections to subclasses */
  gboolean push_data;
  gboolean push_section;
  gboolean push_unknown;

  /* Whether the parent bin is streams-aware, meaning we can
   * add/remove streams at any point in time */
  gboolean streams_aware;

  /* Do not use the PCR stream for timestamp calculation. Useful for
   * streams with broken/invalid PCR streams. */
  gboolean ignore_pcr;

  /* Used for delayed seek events */
  GstEvent *seek_event;
};

struct _MpegTSBaseClass {
  GstElementClass parent_class;

  /* Virtual methods */
  void (*reset) (MpegTSBase *base);
  GstFlowReturn (*push) (MpegTSBase *base, MpegTSPacketizerPacket *packet, GstMpegtsSection * section);
  void (*inspect_packet) (MpegTSBase *base, MpegTSPacketizerPacket *packet);
  /* takes ownership of @event */
  gboolean (*push_event) (MpegTSBase *base, GstEvent * event);
  void (*handle_psi) (MpegTSBase *base, GstMpegtsSection * section);

  /* program_started gets called when program's pmt arrives for first time */
  void (*program_started) (MpegTSBase *base, MpegTSBaseProgram *program);
  /* program_stopped gets called when pat no longer has program's pmt */
  void (*program_stopped) (MpegTSBase *base, MpegTSBaseProgram *program);
  void (*update_program) (MpegTSBase *base, MpegTSBaseProgram *program);
  /* Whether mpegtbase can deactivate/free a program or whether the subclass will do it
   * If the subclass responds TRUE, it should call mpegts_base_deactivate_and_free_program()
   * when it wants to remove it */
  gboolean (*can_remove_program) (MpegTSBase *base, MpegTSBaseProgram *program);

  /* stream_added is called whenever a new stream has been identified */
  gboolean (*stream_added) (MpegTSBase *base, MpegTSBaseStream *stream, MpegTSBaseProgram *program);
  /* stream_removed is called whenever a stream is no longer referenced */
  void (*stream_removed) (MpegTSBase *base, MpegTSBaseStream *stream);

  /* find_timestamps is called to find PCR */
  GstFlowReturn (*find_timestamps) (MpegTSBase * base, guint64 initoff, guint64 *offset);

  /* seek is called to wait for seeking */
  GstFlowReturn (*seek) (MpegTSBase * base, GstEvent * event);

  /* Drain all currently pending data */
  GstFlowReturn (*drain) (MpegTSBase * base);

  /* flush all streams
   * The hard inicator is used to flush completely on FLUSH_STOP events
   * or partially in pull mode seeks of tsdemux */
  void (*flush) (MpegTSBase * base, gboolean hard);

  /* Notifies subclasses input buffer has been handled */
  GstFlowReturn (*input_done) (MpegTSBase *base);

  /* signals */
  void (*pat_info) (GstStructure *pat);
  void (*pmt_info) (GstStructure *pmt);
  void (*nit_info) (GstStructure *nit);
  void (*sdt_info) (GstStructure *sdt);
  void (*eit_info) (GstStructure *eit);

  /* takes ownership of @query */
  gboolean (*sink_query) (MpegTSBase *base, GstQuery * query);
};

#define MPEGTS_BIT_SET(field, offs)    ((field)[(offs) >> 3] |=  (1 << ((offs) & 0x7)))
#define MPEGTS_BIT_UNSET(field, offs)  ((field)[(offs) >> 3] &= ~(1 << ((offs) & 0x7)))
#define MPEGTS_BIT_IS_SET(field, offs) ((field)[(offs) >> 3] &   (1 << ((offs) & 0x7)))

G_GNUC_INTERNAL GType mpegts_base_get_type(void);

G_GNUC_INTERNAL MpegTSBaseProgram *mpegts_base_get_program (MpegTSBase * base, gint program_number);
G_GNUC_INTERNAL MpegTSBaseProgram *mpegts_base_add_program (MpegTSBase * base, gint program_number, guint16 pmt_pid);

G_GNUC_INTERNAL const GstMpegtsDescriptor *mpegts_get_descriptor_from_stream (MpegTSBaseStream * stream, guint8 tag);
G_GNUC_INTERNAL const GstMpegtsDescriptor *mpegts_get_descriptor_from_stream_with_extension (MpegTSBaseStream * stream,
    guint8 tag, guint8 tag_extension);
G_GNUC_INTERNAL const GstMpegtsDescriptor *mpegts_get_descriptor_from_program (MpegTSBaseProgram * program, guint8 tag);

G_GNUC_INTERNAL gboolean
mpegts_base_handle_seek_event(MpegTSBase * base, GstPad * pad, GstEvent * event);

G_GNUC_INTERNAL gboolean gst_mpegtsbase_plugin_init (GstPlugin * plugin);

G_GNUC_INTERNAL void mpegts_base_deactivate_and_free_program (MpegTSBase *base, MpegTSBaseProgram *program);

G_END_DECLS

#endif /* GST_MPEG_TS_BASE_H */
