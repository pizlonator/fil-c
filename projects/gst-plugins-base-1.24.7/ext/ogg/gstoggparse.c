/* GStreamer
 * Copyright (C) 2005 Michael Smith <msmith@fluendo.com>
 *
 * gstoggparse.c: ogg stream parser
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

/* This ogg parser is essentially a subset of the ogg demuxer - rather than
 * fully demuxing into packets, we only parse out the pages, create one
 * GstBuffer per page, set all the appropriate flags on those pages, set caps
 * appropriately (particularly the 'streamheader' which gives all the header
 * pages required for initialing decode).
 *
 * It's dramatically simpler than the full demuxer as it does not  support 
 * seeking.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst.h>
#include <ogg/ogg.h>
#include <string.h>

#include "gstoggelements.h"
#include "gstoggstream.h"

GST_DEBUG_CATEGORY_STATIC (gst_ogg_parse_debug);
#define GST_CAT_DEFAULT gst_ogg_parse_debug

#define GST_TYPE_OGG_PARSE (gst_ogg_parse_get_type())
#define GST_OGG_PARSE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OGG_PARSE, GstOggParse))
#define GST_OGG_PARSE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OGG_PARSE, GstOggParse))
#define GST_IS_OGG_PARSE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OGG_PARSE))
#define GST_IS_OGG_PARSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OGG_PARSE))

static GType gst_ogg_parse_get_type (void);

typedef struct _GstOggParse GstOggParse;
typedef struct _GstOggParseClass GstOggParseClass;

struct _GstOggParse
{
  GstElement element;

  GstPad *sinkpad;              /* Sink pad we're reading data from */

  GstPad *srcpad;               /* Source pad we're writing to */

  GSList *oggstreams;           /* list of GstOggStreams for known streams */

  gint64 offset;                /* Current stream offset */

  gboolean in_headers;          /* Set if we're reading headers for streams */

  gboolean last_page_not_bos;   /* Set if we've seen a non-BOS page */

  ogg_sync_state sync;          /* Ogg page synchronisation */

  GstCaps *caps;                /* Our src caps */

  GstOggStream *video_stream;   /* Stream used to construct delta_unit flags */
};

struct _GstOggParseClass
{
  GstElementClass parent_class;
};


static GstElementClass *parent_class = NULL;
G_DEFINE_TYPE (GstOggParse, gst_ogg_parse, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (oggparse, "oggparse", GST_RANK_NONE,
    GST_TYPE_OGG_PARSE, GST_DEBUG_CATEGORY_INIT (gst_ogg_parse_debug,
        "oggparse", 0, "ogg parser"));

static void
free_stream (GstOggStream * stream)
{
  g_list_foreach (stream->headers, (GFunc) gst_mini_object_unref, NULL);
  g_list_foreach (stream->unknown_pages, (GFunc) gst_mini_object_unref, NULL);
  g_list_foreach (stream->stored_buffers, (GFunc) gst_mini_object_unref, NULL);

  g_free (stream);
}

static void
gst_ogg_parse_delete_all_streams (GstOggParse * ogg)
{
  g_slist_foreach (ogg->oggstreams, (GFunc) free_stream, NULL);
  g_slist_free (ogg->oggstreams);
  ogg->oggstreams = NULL;
}

static GstOggStream *
gst_ogg_parse_new_stream (GstOggParse * parser, ogg_page * page)
{
  GstOggStream *stream;
  ogg_packet packet;
  int ret;
  guint32 serialno;

  serialno = ogg_page_serialno (page);

  GST_DEBUG_OBJECT (parser, "creating new stream %08x", serialno);

  stream = g_new0 (GstOggStream, 1);

  stream->serialno = serialno;
  stream->in_headers = 1;

  if (ogg_stream_init (&stream->stream, serialno) != 0) {
    GST_ERROR ("Could not initialize ogg_stream struct for serial %08x.",
        serialno);
    goto failure;
  }

  if (ogg_stream_pagein (&stream->stream, page) != 0)
    goto failure;

  ret = ogg_stream_packetout (&stream->stream, &packet);
  if (ret == 1) {
    if (!gst_ogg_stream_setup_map (stream, &packet)) {
      GST_ERROR ("Could not setup map for ogg packet.");
      goto failure;
    }

    if (stream->is_video) {
      parser->video_stream = stream;
    }
  }

  parser->oggstreams = g_slist_append (parser->oggstreams, stream);

  return stream;

failure:
  free_stream (stream);
  return NULL;
}

static GstOggStream *
gst_ogg_parse_find_stream (GstOggParse * parser, guint32 serialno)
{
  GSList *l;

  for (l = parser->oggstreams; l != NULL; l = l->next) {
    GstOggStream *stream = (GstOggStream *) l->data;

    if (stream->serialno == serialno)
      return stream;
  }
  return NULL;
}

/* signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0
      /* FILL ME */
};

static GstStaticPadTemplate ogg_parse_src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/ogg")
    );

static GstStaticPadTemplate ogg_parse_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/ogg")
    );

static void gst_ogg_parse_dispose (GObject * object);
static GstStateChangeReturn gst_ogg_parse_change_state (GstElement * element,
    GstStateChange transition);
static GstFlowReturn gst_ogg_parse_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);

static void
gst_ogg_parse_class_init (GstOggParseClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_set_static_metadata (gstelement_class,
      "Ogg parser", "Codec/Parser",
      "parse ogg streams into pages (info about ogg: http://xiph.org)",
      "Michael Smith <msmith@fluendo.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &ogg_parse_sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &ogg_parse_src_template_factory);

  parent_class = g_type_class_peek_parent (klass);

  gstelement_class->change_state = gst_ogg_parse_change_state;

  gobject_class->dispose = gst_ogg_parse_dispose;
}

static void
gst_ogg_parse_init (GstOggParse * ogg)
{
  /* create the sink and source pads */
  ogg->sinkpad =
      gst_pad_new_from_static_template (&ogg_parse_sink_template_factory,
      "sink");
  ogg->srcpad =
      gst_pad_new_from_static_template (&ogg_parse_src_template_factory, "src");

  /* TODO: Are there any events we must handle? */
  /* gst_pad_set_event_function (ogg->sinkpad, gst_ogg_parse_handle_event); */
  gst_pad_set_chain_function (ogg->sinkpad, gst_ogg_parse_chain);

  gst_element_add_pad (GST_ELEMENT (ogg), ogg->sinkpad);
  gst_element_add_pad (GST_ELEMENT (ogg), ogg->srcpad);

  ogg->oggstreams = NULL;
}

static void
gst_ogg_parse_dispose (GObject * object)
{
  GstOggParse *ogg = GST_OGG_PARSE (object);

  GST_LOG_OBJECT (ogg, "Disposing of object %p", ogg);

  ogg_sync_clear (&ogg->sync);
  gst_ogg_parse_delete_all_streams (ogg);

  if (ogg->caps) {
    gst_caps_unref (ogg->caps);
    ogg->caps = NULL;
  }

  if (G_OBJECT_CLASS (parent_class)->dispose)
    G_OBJECT_CLASS (parent_class)->dispose (object);
}

/* submit the given buffer to the ogg sync */
static GstFlowReturn
gst_ogg_parse_submit_buffer (GstOggParse * ogg, GstBuffer * buffer)
{
  gsize size;
  gchar *oggbuffer;
  GstFlowReturn ret = GST_FLOW_OK;

  size = gst_buffer_get_size (buffer);

  GST_DEBUG_OBJECT (ogg, "submitting %" G_GSIZE_FORMAT " bytes", size);
  if (G_UNLIKELY (size == 0))
    goto done;

  oggbuffer = ogg_sync_buffer (&ogg->sync, size);
  if (G_UNLIKELY (oggbuffer == NULL)) {
    GST_ELEMENT_ERROR (ogg, STREAM, DECODE,
        (NULL), ("failed to get ogg sync buffer"));
    ret = GST_FLOW_ERROR;
    goto done;
  }

  size = gst_buffer_extract (buffer, 0, oggbuffer, size);
  if (G_UNLIKELY (ogg_sync_wrote (&ogg->sync, size) < 0)) {
    GST_ELEMENT_ERROR (ogg, STREAM, DECODE, (NULL),
        ("failed to write %" G_GSIZE_FORMAT " bytes to the sync buffer", size));
    ret = GST_FLOW_ERROR;
  }

done:
  gst_buffer_unref (buffer);

  return ret;
}

static void
gst_ogg_parse_append_header (GValue * array, GstBuffer * buf)
{
  GValue value = { 0 };
  /* We require a copy to avoid circular refcounts */
  GstBuffer *buffer = gst_buffer_copy (buf);

  GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_HEADER);

  g_value_init (&value, GST_TYPE_BUFFER);
  gst_value_set_buffer (&value, buffer);
  gst_value_array_append_value (array, &value);
  g_value_unset (&value);

}

typedef enum
{
  PAGE_HEADER,                  /* Header page */
  PAGE_DATA,                    /* Data page */
  PAGE_PENDING,                 /* We don't know yet, we'll have to see some future pages */
} page_type;

static page_type
gst_ogg_parse_is_header (GstOggParse * ogg, GstOggStream * stream,
    ogg_page * page)
{
  ogg_int64_t gpos = ogg_page_granulepos (page);

  if (gpos < 0)
    return PAGE_PENDING;

  /* This is good enough for now, but technically requires codec-specific
   * behaviour to be perfect. This is where we need the mooted library for 
   * this stuff, which nobody has written.
   */
  if (gpos > 0)
    return PAGE_DATA;
  else
    return PAGE_HEADER;
}

static GstBuffer *
gst_ogg_parse_buffer_from_page (ogg_page * page,
    guint64 offset, GstClockTime timestamp)
{
  int size = page->header_len + page->body_len;
  GstBuffer *buf = gst_buffer_new_and_alloc (size);

  gst_buffer_fill (buf, 0, page->header, page->header_len);
  gst_buffer_fill (buf, page->header_len, page->body, page->body_len);

  GST_BUFFER_TIMESTAMP (buf) = timestamp;
  GST_BUFFER_OFFSET (buf) = offset;
  GST_BUFFER_OFFSET_END (buf) = offset + size;

  return buf;
}


/* Reads in buffers, parses them, reframes into one-buffer-per-ogg-page, submits
 * pages to output pad.
 */
static GstFlowReturn
gst_ogg_parse_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstOggParse *ogg;
  GstFlowReturn result = GST_FLOW_OK;
  gint ret = -1;
  guint32 serialno;
  GstBuffer *pagebuffer;
  GstClockTime buffertimestamp = GST_BUFFER_TIMESTAMP (buffer);

  ogg = GST_OGG_PARSE (parent);

  GST_LOG_OBJECT (ogg,
      "Chain function received buffer of size %" G_GSIZE_FORMAT,
      gst_buffer_get_size (buffer));

  gst_ogg_parse_submit_buffer (ogg, buffer);

  while (ret != 0 && result == GST_FLOW_OK) {
    ogg_page page;

    /* We use ogg_sync_pageseek() rather than ogg_sync_pageout() so that we can
     * track how many bytes the ogg layer discarded (in the case of sync errors,
     * etc.); this allows us to accurately track the current stream offset
     */
    ret = ogg_sync_pageseek (&ogg->sync, &page);
    if (ret == 0) {
      /* need more data, that's fine... */
      break;
    } else if (ret < 0) {
      /* discontinuity; track how many bytes we skipped (-ret) */
      ogg->offset -= ret;
    } else {
      gint64 granule = ogg_page_granulepos (&page);
#ifndef GST_DISABLE_GST_DEBUG
      int bos = ogg_page_bos (&page);
#endif
      guint64 startoffset = ogg->offset;
      GstOggStream *stream;
      gboolean keyframe;

      serialno = ogg_page_serialno (&page);
      stream = gst_ogg_parse_find_stream (ogg, serialno);

      GST_LOG_OBJECT (ogg, "Timestamping outgoing buffer as %" GST_TIME_FORMAT,
          GST_TIME_ARGS (buffertimestamp));

      if (stream) {
        buffertimestamp = gst_ogg_stream_get_end_time_for_granulepos (stream,
            granule);
        if (ogg->video_stream) {
          if (stream == ogg->video_stream) {
            keyframe = gst_ogg_stream_granulepos_is_key_frame (stream, granule);
          } else {
            keyframe = FALSE;
          }
        } else {
          keyframe = TRUE;
        }
      } else {
        buffertimestamp = GST_CLOCK_TIME_NONE;
        keyframe = TRUE;
      }
      pagebuffer = gst_ogg_parse_buffer_from_page (&page, startoffset,
          buffertimestamp);

      /* We read out 'ret' bytes, so we set the next offset appropriately */
      ogg->offset += ret;

      GST_LOG_OBJECT (ogg,
          "processing ogg page (serial %08x, pageno %ld, "
          "granule pos %" G_GUINT64_FORMAT ", bos %d, offset %"
          G_GUINT64_FORMAT "-%" G_GUINT64_FORMAT ") keyframe=%d",
          serialno, ogg_page_pageno (&page),
          granule, bos, startoffset, ogg->offset, keyframe);

      if (ogg_page_bos (&page)) {
        /* If we've seen this serialno before, this is technically an error,
         * we log this case but accept it - this one replaces the previous
         * stream with this serialno. We can do this since we're streaming, and
         * not supporting seeking...
         */
        GstOggStream *stream = gst_ogg_parse_find_stream (ogg, serialno);

        if (stream != NULL) {
          GST_LOG_OBJECT (ogg, "Incorrect stream; repeats serial number %08x "
              "at offset %" G_GINT64_FORMAT, serialno, ogg->offset);
        }

        if (ogg->last_page_not_bos) {
          GST_LOG_OBJECT (ogg, "Deleting all referenced streams, found a new "
              "chain starting with serial %u", serialno);
          gst_ogg_parse_delete_all_streams (ogg);
        }

        stream = gst_ogg_parse_new_stream (ogg, &page);
        if (!stream) {
          GST_LOG_OBJECT (ogg, "Incorrect page");
          goto failure;
        }

        ogg->last_page_not_bos = FALSE;

        gst_buffer_ref (pagebuffer);
        stream->headers = g_list_append (stream->headers, pagebuffer);

        if (!ogg->in_headers) {
          GST_LOG_OBJECT (ogg,
              "Found start of new chain at offset %" G_GUINT64_FORMAT,
              startoffset);
          ogg->in_headers = 1;
        }

        /* For now, we just keep the header buffer in the stream->headers list;
         * it actually gets output once we've collected the entire set
         */
      } else {
        /* Non-BOS page. Either: we're outside headers, and this isn't a 
         * header (normal data), outside headers and this is (error!), inside
         * headers, this is (append header), or inside headers and this isn't 
         * (we've found the end of headers; flush the lot!)
         *
         * Before that, we flag that the last page seen (this one) was not a 
         * BOS page; that way we know that when we next see a BOS page it's a
         * new chain, and we can flush all existing streams.
         */
        page_type type;
        GstOggStream *stream = gst_ogg_parse_find_stream (ogg, serialno);

        if (!stream) {
          GST_LOG_OBJECT (ogg,
              "Non-BOS page unexpectedly found at %" G_GINT64_FORMAT,
              ogg->offset);
          goto failure;
        }

        ogg->last_page_not_bos = TRUE;

        type = gst_ogg_parse_is_header (ogg, stream, &page);

        if (type == PAGE_PENDING && ogg->in_headers) {
          gst_buffer_ref (pagebuffer);

          stream->unknown_pages = g_list_append (stream->unknown_pages,
              pagebuffer);
        } else if (type == PAGE_HEADER) {
          if (!ogg->in_headers) {
            GST_LOG_OBJECT (ogg, "Header page unexpectedly found outside "
                "headers at offset %" G_GINT64_FORMAT, ogg->offset);
            goto failure;
          } else {
            /* Append the header to the buffer list, after any unknown previous
             * pages
             */
            stream->headers = g_list_concat (stream->headers,
                stream->unknown_pages);
            g_list_free (stream->unknown_pages);
            gst_buffer_ref (pagebuffer);
            stream->headers = g_list_append (stream->headers, pagebuffer);
          }
        } else {                /* PAGE_DATA, or PAGE_PENDING but outside headers */
          if (ogg->in_headers) {
            /* First non-header page... set caps, flush headers.
             *
             * First up, we build a single GValue list of all the pagebuffers
             * we're using for the headers, in order.
             * Then we set this on the caps structure. Then we can start pushing
             * buffers for the headers, and finally we send this non-header
             * page.
             */
            GstCaps *caps;
            GstStructure *structure;
            GValue array = { 0 };
            gint count = 0;
            gboolean found_pending_headers = FALSE;
            GSList *l;

            g_value_init (&array, GST_TYPE_ARRAY);

            for (l = ogg->oggstreams; l != NULL; l = l->next) {
              GstOggStream *stream = (GstOggStream *) l->data;

              if (g_list_length (stream->headers) == 0) {
                GST_LOG_OBJECT (ogg, "No primary header found for stream %08x",
                    stream->serialno);
                goto failure;
              }

              gst_ogg_parse_append_header (&array,
                  GST_BUFFER (stream->headers->data));
              count++;
            }

            for (l = ogg->oggstreams; l != NULL; l = l->next) {
              GstOggStream *stream = (GstOggStream *) l->data;
              GList *j;

              /* already appended the first header, now do headers 2-N */
              for (j = stream->headers->next; j != NULL; j = j->next) {
                gst_ogg_parse_append_header (&array, GST_BUFFER (j->data));
                count++;
              }
            }

            caps = gst_pad_query_caps (ogg->srcpad, NULL);
            caps = gst_caps_make_writable (caps);

            structure = gst_caps_get_structure (caps, 0);
            gst_structure_take_value (structure, "streamheader", &array);

            gst_pad_set_caps (ogg->srcpad, caps);

            if (ogg->caps)
              gst_caps_unref (ogg->caps);
            ogg->caps = caps;

            GST_LOG_OBJECT (ogg, "Set \"streamheader\" caps with %d buffers "
                "(one per page)", count);

            /* Now, we do the same thing, but push buffers... */
            for (l = ogg->oggstreams; l != NULL; l = l->next) {
              GstOggStream *stream = (GstOggStream *) l->data;
              GstBuffer *buf = GST_BUFFER (stream->headers->data);

              result = gst_pad_push (ogg->srcpad, buf);
              if (result != GST_FLOW_OK)
                return result;
            }
            for (l = ogg->oggstreams; l != NULL; l = l->next) {
              GstOggStream *stream = (GstOggStream *) l->data;
              GList *j;

              /* pushed the first one for each stream already, now do 2-N */
              for (j = stream->headers->next; j != NULL; j = j->next) {
                GstBuffer *buf = GST_BUFFER (j->data);

                result = gst_pad_push (ogg->srcpad, buf);
                if (result != GST_FLOW_OK)
                  return result;
              }
            }

            ogg->in_headers = 0;

            /* And finally the pending data pages */
            for (l = ogg->oggstreams; l != NULL; l = l->next) {
              GstOggStream *stream = (GstOggStream *) l->data;
              GList *k;

              if (stream->unknown_pages == NULL)
                continue;

              if (found_pending_headers) {
                GST_WARNING_OBJECT (ogg, "Incorrectly muxed headers found at "
                    "approximate offset %" G_GINT64_FORMAT, ogg->offset);
              }
              found_pending_headers = TRUE;

              GST_LOG_OBJECT (ogg, "Pushing %d pending pages after headers",
                  g_list_length (stream->unknown_pages) + 1);

              for (k = stream->unknown_pages; k != NULL; k = k->next) {
                GstBuffer *buf = GST_BUFFER (k->data);

                result = gst_pad_push (ogg->srcpad, buf);
                if (result != GST_FLOW_OK)
                  return result;
              }
              g_list_foreach (stream->unknown_pages,
                  (GFunc) gst_mini_object_unref, NULL);
              g_list_free (stream->unknown_pages);
              stream->unknown_pages = NULL;
            }
          }

          if (granule == -1) {
            stream->stored_buffers = g_list_append (stream->stored_buffers,
                pagebuffer);
          } else {
            while (stream->stored_buffers) {
              GstBuffer *buf = stream->stored_buffers->data;

              buf = gst_buffer_make_writable (buf);

              GST_BUFFER_TIMESTAMP (buf) = buffertimestamp;
              if (!keyframe) {
                GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
              } else {
                keyframe = FALSE;
              }

              result = gst_pad_push (ogg->srcpad, buf);
              if (result != GST_FLOW_OK)
                return result;

              stream->stored_buffers =
                  g_list_delete_link (stream->stored_buffers,
                  stream->stored_buffers);
            }

            pagebuffer = gst_buffer_make_writable (pagebuffer);
            if (!keyframe) {
              GST_BUFFER_FLAG_SET (pagebuffer, GST_BUFFER_FLAG_DELTA_UNIT);
            } else {
              keyframe = FALSE;
            }

            result = gst_pad_push (ogg->srcpad, pagebuffer);
            if (result != GST_FLOW_OK)
              return result;
          }
        }
      }
    }
  }

  return result;

failure:
  gst_pad_push_event (GST_PAD (ogg->srcpad), gst_event_new_eos ());
  return GST_FLOW_ERROR;
}

static GstStateChangeReturn
gst_ogg_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstOggParse *ogg;
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  ogg = GST_OGG_PARSE (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      ogg_sync_init (&ogg->sync);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ogg_sync_reset (&ogg->sync);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  result = parent_class->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      ogg_sync_clear (&ogg->sync);
      break;
    default:
      break;
  }
  return result;
}
