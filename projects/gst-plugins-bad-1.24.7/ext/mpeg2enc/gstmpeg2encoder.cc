/* GStreamer mpeg2enc (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2006 Mark Nauwelaerts <manauw@skynet.be>
 *
 * gstmpeg2encoder.cc: gstreamer/mpeg2enc encoder class
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

#include <mpegconsts.h>
#include <quantize.hh>
#if GST_MJPEGTOOLS_API >= 20000
#include <ontheflyratectlpass1.hh>
#include <ontheflyratectlpass2.hh>
#elif GST_MJPEGTOOLS_API >= 10900
#include <ontheflyratectl.hh>
#include <pass1ratectl.hh>
#include <pass2ratectl.hh>
#endif
#include <seqencoder.hh>
#include <mpeg2coder.hh>

#include "gstmpeg2enc.hh"
#include "gstmpeg2encoder.hh"

class GstOnTheFlyPass2 : public OnTheFlyPass2 {
  public:
    GstOnTheFlyPass2 (EncoderParams &encoder, gboolean disable_encode_retries): OnTheFlyPass2(encoder), disable_encode_retries(disable_encode_retries) {}
    bool ReencodeRequired() const { return disable_encode_retries ? false : OnTheFlyPass2::ReencodeRequired(); }
  private:
    gboolean disable_encode_retries;
};

/*
 * Class init stuff.
 */

GstMpeg2Encoder::GstMpeg2Encoder (GstMpeg2EncOptions * options, GstElement * in_element, GstCaps * in_caps):
MPEG2Encoder (*options)
{
  element = in_element;
  gst_object_ref (element);
  caps = in_caps;
  gst_caps_ref (in_caps);
  init_done = FALSE;
  disable_encode_retries = options->disable_encode_retries;
}

GstMpeg2Encoder::~GstMpeg2Encoder ()
{
  gst_caps_unref (caps);
  gst_object_unref (element);
}

gboolean GstMpeg2Encoder::setup ()
{
  MPEG2EncInVidParams
      strm;
  GstVideoEncoder *
      video_encoder;

  video_encoder = GST_VIDEO_ENCODER (element);

  /* I/O */
  reader = new GstMpeg2EncPictureReader (element, caps, &parms);
  reader->StreamPictureParams (strm);
  if (options.SetFormatPresets (strm)) {
    delete reader;
    reader = NULL;
    writer = NULL;
    quantizer = NULL;
    pass1ratectl = NULL;
    pass2ratectl = NULL;
    /* sequencer */
    seqencoder = NULL;

    return FALSE;
  }
  writer = new GstMpeg2EncStreamWriter (video_encoder, &parms);

  /* encoding internals */
  quantizer = new Quantizer (parms);
  pass1ratectl = new OnTheFlyPass1 (parms);
  pass2ratectl = new GstOnTheFlyPass2 (parms, disable_encode_retries);
  /* sequencer */
  seqencoder = new SeqEncoder (parms, *reader, *quantizer,
      *writer, *pass1ratectl, *pass2ratectl);

  return TRUE;
}

void
GstMpeg2Encoder::init ()
{
  if (!init_done) {
    parms.Init (options);
    reader->Init ();
    quantizer->Init ();
    seqencoder->Init ();
    init_done = TRUE;
  }
}

/*
 * Process all input provided by the reader until it signals eos.
 */

void
GstMpeg2Encoder::encode ()
{
  /* hm, this is all... eek! */
  seqencoder->EncodeStream ();
}

/*
 * Get current output format.
 */

GstCaps *
GstMpeg2Encoder::getFormat ()
{
  y4m_ratio_t fps = mpeg_framerate (options.frame_rate);

  return gst_caps_new_simple ("video/mpeg",
      "systemstream", G_TYPE_BOOLEAN, FALSE,
      "mpegversion", G_TYPE_INT, options.mpeg,
      "width", G_TYPE_INT, options.in_img_width,
      "height", G_TYPE_INT, options.in_img_height,
      "framerate", GST_TYPE_FRACTION, fps.n, fps.d, NULL);
}
