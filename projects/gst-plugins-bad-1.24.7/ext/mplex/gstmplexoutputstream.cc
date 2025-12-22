/* GStreamer mplex (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2008 Mark Nauwelaerts <mnauw@users.sourceforge.net>
 *
 * gstmplexoutputstream.hh: gstreamer/mplex output stream wrapper
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
#include "config.h"
#endif

#include <string.h>

#include "gstmplex.hh"
#include "gstmplexoutputstream.hh"

/*
 * Class init functions.
 */

GstMplexOutputStream::GstMplexOutputStream (GstMplex * _element, GstPad * _pad):
OutputStream ()
{
  mplex = _element;
  pad = _pad;
  size = 0;
}

/*
 * Open/close. Basically 'no-op's (close() sets EOS).
 *
 * Open (): -1 means failure, 0 means success.
 */

int
GstMplexOutputStream::Open (void)
{
  return 0;
}

void
GstMplexOutputStream::Close (void)
{
  GST_MPLEX_MUTEX_LOCK (mplex);
  GST_DEBUG_OBJECT (mplex, "closing stream and sending eos");
  gst_pad_push_event (pad, gst_event_new_eos ());
  /* notify chain there is no more need to supply buffers */
  mplex->eos = TRUE;
  GST_MPLEX_SIGNAL_ALL (mplex);
  GST_MPLEX_MUTEX_UNLOCK (mplex);
}

/*
 * Get size of current segment.
 */

#if GST_MJPEGTOOLS_API >= 10900
uint64_t
GstMplexOutputStream::SegmentSize (void)
#else
off_t
GstMplexOutputStream::SegmentSize (void)
#endif
{
  return size;
}

/*
 * Next segment; not really supported.
 */

void
GstMplexOutputStream::NextSegment (void)
{
  size = 0;

  GST_WARNING_OBJECT (mplex, "multiple file output is not supported");
  /* FIXME: no such filesink behaviour to be expected */
}

/*
 * Write data.
 */

void
GstMplexOutputStream::Write (guint8 * data, guint len)
{
  GstBuffer *buf;

  buf = gst_buffer_new_and_alloc (len);
  gst_buffer_fill (buf, 0, data, len);

  size += len;
  GST_MPLEX_MUTEX_LOCK (mplex);
  mplex->srcresult = gst_pad_push (pad, buf);
  GST_MPLEX_MUTEX_UNLOCK (mplex);
}
