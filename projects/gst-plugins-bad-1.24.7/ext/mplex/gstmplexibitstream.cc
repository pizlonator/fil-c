/* GStreamer mplex (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2008 Mark Nauwelaerts <mnauw@users.sourceforge.net>
 *
 * gstmplexibitstream.hh: gstreamer/mplex input bitstream wrapper
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
#include "gstmplexibitstream.hh"

/*
 * Class init/exit functions.
 */

GstMplexIBitStream::GstMplexIBitStream (GstMplexPad * _data, guint buf_size):
IBitStream ()
{
  mpad = _data;
  mplex = GST_MPLEX (GST_PAD_PARENT (mpad->pad));
  eos = FALSE;

  SetBufSize (buf_size);
  eobs = false;
  byteidx = 0;
}

/*
 * Read data.
 */

size_t
    GstMplexIBitStream::ReadStreamBytes (uint8_t * buf, size_t size =
    BUFFER_SIZE)
{
  gpointer data;

  GST_MPLEX_MUTEX_LOCK (mplex);

  GST_DEBUG_OBJECT (mplex, "needing %d bytes", (guint) size);

  while (gst_adapter_available (mpad->adapter) < size
      && !mplex->eos && !mpad->eos) {
    mpad->needed = size;
    GST_MPLEX_SIGNAL (mplex, mpad);
    GST_MPLEX_WAIT (mplex, mpad);
  }

  mpad->needed = 0;
  size = MIN (size, gst_adapter_available (mpad->adapter));
  if (size) {
    data = gst_adapter_take (mpad->adapter, size);
    memcpy (buf, data, size);
    g_free (data);
  }

  GST_MPLEX_MUTEX_UNLOCK (mplex);

  return size;
}

/*
 * Are we at EOS?
 */

bool GstMplexIBitStream::EndOfStream (void)
{
  return eos;
}

bool GstMplexIBitStream::ReadBuffer ()
{
  return ReadIntoBuffer (BUFFER_SIZE);
}
