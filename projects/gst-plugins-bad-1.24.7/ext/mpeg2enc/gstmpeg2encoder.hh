/* GStreamer mpeg2enc (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2006 Mark Nauwelaerts <manauw@skynet.be>
 *
 * gstmpeg2encoder.hh: gstreamer/mpeg2enc encoder class
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

#ifndef __GST_MPEG2ENCODER_H__
#define __GST_MPEG2ENCODER_H__


#include <mpeg2encoder.hh>
#include "gstmpeg2encoptions.hh"
#include "gstmpeg2encpicturereader.hh"
#include "gstmpeg2encstreamwriter.hh"

class GstMpeg2Encoder : public MPEG2Encoder {
public:
  GstMpeg2Encoder (GstMpeg2EncOptions *options, 
      GstElement *element, GstCaps *caps);
  ~GstMpeg2Encoder ();

  gboolean setup ();
  void init ();

  /* process stream */
  void encode ();

  /* get current output format */
  GstCaps *getFormat ();

private:
  GstElement *element;
  GstCaps *caps;
  gboolean init_done;
  gboolean disable_encode_retries;
};

#endif /* __GST_MPEG2ENCODER_H__ */
