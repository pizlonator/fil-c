/*
 * DASH MPD parsing library
 *
 * gstmpdparser.h
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * Authors:
 *   Gianluca Gennari <gennarone@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_MPDPARSER_H__
#define __GST_MPDPARSER_H__

#include <gst/gst.h>
#include <gst/uridownloader/gsturidownloader.h>
#include <gst/base/gstadapter.h>
#include "gstmpdhelper.h"
#include "gstxmlhelper.h"
#include "gstmpdrootnode.h"
#include "gstmpdbaseurlnode.h"
#include "gstmpdutctimingnode.h"
#include "gstmpdmetricsnode.h"
#include "gstmpdmetricsrangenode.h"
#include "gstmpdsnode.h"
#include "gstmpdsegmenttimelinenode.h"
#include "gstmpdsegmenttemplatenode.h"
#include "gstmpdsegmenturlnode.h"
#include "gstmpdsegmentlistnode.h"
#include "gstmpdsegmentbasenode.h"
#include "gstmpdperiodnode.h"
#include "gstmpdrepresentationnode.h"
#include "gstmpdsubrepresentationnode.h"
#include "gstmpdcontentcomponentnode.h"
#include "gstmpdadaptationsetnode.h"
#include "gstmpdsubsetnode.h"
#include "gstmpdprograminformationnode.h"
#include "gstmpdlocationnode.h"
#include "gstmpdreportingnode.h"
#include "gstmpdurltypenode.h"
#include "gstmpddescriptortypenode.h"
#include "gstmpdrepresentationbasenode.h"
#include "gstmpdmultsegmentbasenode.h"

G_BEGIN_DECLS

typedef struct _GstActiveStream           GstActiveStream;
typedef struct _GstStreamPeriod           GstStreamPeriod;
typedef struct _GstMediaFragmentInfo      GstMediaFragmentInfo;
typedef struct _GstMediaSegment           GstMediaSegment;


#define GST_MPD_DURATION_NONE ((guint64)-1)

typedef enum
{
  GST_STREAM_UNKNOWN,
  GST_STREAM_VIDEO,           /* video stream (the main one) */
  GST_STREAM_AUDIO,           /* audio stream (optional) */
  GST_STREAM_APPLICATION      /* application stream (optional): for timed text/subtitles */
} GstStreamMimeType;

/**
 * GstStreamPeriod:
 *
 * Stream period data structure
 */
struct _GstStreamPeriod
{
  GstMPDPeriodNode *period;                      /* Stream period */
  guint number;                               /* Period number */
  GstClockTime start;                         /* Period start time */
  GstClockTime duration;                      /* Period duration */
};

/**
 * GstMediaSegment:
 *
 * Media segment data structure
 */
struct _GstMediaSegment
{
  GstMPDSegmentURLNode *SegmentURL;              /* this is NULL when using a SegmentTemplate */
  guint number;                               /* segment number */
  gint repeat;                                /* number of extra repetitions (0 = played only once) */
  guint64 scale_start;                        /* start time in timescale units */
  guint64 scale_duration;                     /* duration in timescale units */
  GstClockTime start;                         /* segment start time */
  GstClockTime duration;                      /* segment duration */
};

struct _GstMediaFragmentInfo
{
  gchar *uri;
  gint64 range_start;
  gint64 range_end;

  gchar *index_uri;
  gint64 index_range_start;
  gint64 index_range_end;

  gboolean discontinuity;
  GstClockTime timestamp;
  GstClockTime duration;
};

/**
 * GstActiveStream:
 *
 * Active stream data structure
 */
struct _GstActiveStream
{
  GstStreamMimeType mimeType;                 /* video/audio/application */

  guint baseURL_idx;                          /* index of the baseURL used for last request */
  gchar *baseURL;                             /* active baseURL used for last request */
  gchar *queryURL;                            /* active queryURL used for last request */
  guint max_bandwidth;                        /* max bandwidth allowed for this mimeType */

  GstMPDAdaptationSetNode *cur_adapt_set;        /* active adaptation set */
  gint representation_idx;                    /* index of current representation */
  GstMPDRepresentationNode *cur_representation;  /* active representation */
  GstMPDSegmentBaseNode *cur_segment_base;       /* active segment base */
  GstMPDSegmentListNode *cur_segment_list;       /* active segment list */
  GstMPDSegmentTemplateNode *cur_seg_template;   /* active segment template */
  gint segment_index;                         /* index of next sequence chunk */
  guint segment_repeat_index;                 /* index of the repeat count of a segment */
  GPtrArray *segments;                        /* array of GstMediaSegment */
  GstClockTime presentationTimeOffset;        /* presentation time offset of the current segment */
};

/* MPD file parsing */
gboolean gst_mpdparser_get_mpd_root_node (GstMPDRootNode ** mpd_root_node, const gchar * data, gint size);
GstMPDSegmentListNode * gst_mpdparser_get_external_segment_list (const gchar * data, gint size, GstMPDSegmentListNode * parent);
GList * gst_mpdparser_get_external_periods (const gchar * data, gint size);
GList * gst_mpdparser_get_external_adaptation_sets (const gchar * data, gint size, GstMPDPeriodNode* period);

/* navigation functions */
GstStreamMimeType gst_mpdparser_representation_get_mimetype (GstMPDAdaptationSetNode * adapt_set, GstMPDRepresentationNode * rep);

/* Memory management */
void gst_mpdparser_free_stream_period (GstStreamPeriod * stream_period);
void gst_mpdparser_free_media_segment (GstMediaSegment * media_segment);
void gst_mpdparser_free_active_stream (GstActiveStream * active_stream);
void gst_mpdparser_media_fragment_info_clear (GstMediaFragmentInfo * fragment);
/* Active stream methods*/
void gst_mpdparser_init_active_stream_segments (GstActiveStream * stream);
gchar *gst_mpdparser_get_mediaURL (GstActiveStream * stream, GstMPDSegmentURLNode * segmentURL);
gchar *gst_mpdparser_get_initializationURL (GstActiveStream * stream, GstMPDURLTypeNode * InitializationURL);
gchar *gst_mpdparser_build_URL_from_template (const gchar * url_template, const gchar * id, guint number, guint bandwidth, guint64 time);

G_END_DECLS

#endif /* __GST_MPDPARSER_H__ */

