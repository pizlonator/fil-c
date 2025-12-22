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

#ifndef __GST_MPLEXIBITSTREAM_H__
#define __GST_MPLEXIBITSTREAM_H__

#include <gst/gst.h>
#include <mjpeg_types.h>
#include <bits.hpp>

#include "gstmplex.hh"

/* forward declaration; break circular referencing */
typedef struct _GstMplex GstMplex;
typedef struct _GstMplexPad GstMplexPad;

class GstMplexIBitStream : public IBitStream {
public:
  GstMplexIBitStream (GstMplexPad *pad, guint buf_size = BUFFER_SIZE);
  bool ReadBuffer ();

protected:
  /* read data */
  size_t ReadStreamBytes (uint8_t *buf, size_t number);

  /* are we at EOS? */
  bool EndOfStream (void);

private:
  GstMplex *mplex;
  GstMplexPad *mpad;
  gboolean eos;
};

#endif /* __GST_MPLEXIBITSTREAM_H__ */
