/* 
 * Copyright (C) 2013 Sebastian Dröge <sebastian@centricular.com>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstopenexrdec.h"

#include <gst/base/base.h>
#include <string.h>
#include <stdint.h>

#include <ImfRgbaFile.h>
#include <ImfIO.h>
using namespace Imf;
using namespace Imath;

/* Memory stream reader */
class MemIStream:public IStream
{
public:
  MemIStream (const char *file_name, const guint8 * data,
      gsize size):IStream (file_name), data (data), offset (0), size (size)
  {
  }

  virtual bool read (char c[], int n);
  virtual uint64_t tellg ();
  virtual void seekg (uint64_t pos);
  virtual void clear ();

private:
  const guint8 *data;
  gsize offset, size;
};

bool MemIStream::read (char c[], int n)
{
  if (offset + n > size)
    throw
    Iex::InputExc ("Unexpected end of file");

  memcpy (c, data + offset, n);
  offset += n;

  return (offset == size);
}

uint64_t MemIStream::tellg ()
{
  return offset;
}

void
MemIStream::seekg (uint64_t pos)
{
  offset = pos;
  if (offset > size)
    offset = size;
}

void
MemIStream::clear ()
{

}

GST_DEBUG_CATEGORY_STATIC (gst_openexr_dec_debug);
#define GST_CAT_DEFAULT gst_openexr_dec_debug

static gboolean gst_openexr_dec_start (GstVideoDecoder * decoder);
static gboolean gst_openexr_dec_stop (GstVideoDecoder * decoder);
static GstFlowReturn gst_openexr_dec_parse (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstAdapter * adapter, gboolean at_eos);
static gboolean gst_openexr_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_openexr_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static gboolean gst_openexr_dec_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query);

static GstStaticPadTemplate gst_openexr_dec_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/x-exr")
    );

static GstStaticPadTemplate gst_openexr_dec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("ARGB64"))
    );

#define parent_class gst_openexr_dec_parent_class
G_DEFINE_TYPE (GstOpenEXRDec, gst_openexr_dec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (openexrdec, "openexrdec", GST_RANK_PRIMARY,
          GST_TYPE_OPENEXR_DEC);

static void
gst_openexr_dec_class_init (GstOpenEXRDecClass * klass)
{
  GstElementClass *element_class;
  GstVideoDecoderClass *video_decoder_class;

  element_class = (GstElementClass *) klass;
  video_decoder_class = (GstVideoDecoderClass *) klass;

  gst_element_class_add_static_pad_template (element_class, &gst_openexr_dec_src_template);
  gst_element_class_add_static_pad_template (element_class, &gst_openexr_dec_sink_template);

  gst_element_class_set_static_metadata (element_class,
      "OpenEXR decoder",
      "Codec/Decoder/Video",
      "Decode EXR streams", "Sebastian Dröge <sebastian@centricular.com>");

  video_decoder_class->start = GST_DEBUG_FUNCPTR (gst_openexr_dec_start);
  video_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_openexr_dec_stop);
  video_decoder_class->parse = GST_DEBUG_FUNCPTR (gst_openexr_dec_parse);
  video_decoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_openexr_dec_set_format);
  video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_openexr_dec_handle_frame);
  video_decoder_class->decide_allocation = gst_openexr_dec_decide_allocation;

  GST_DEBUG_CATEGORY_INIT (gst_openexr_dec_debug, "openexrdec", 0,
      "OpenEXR Decoder");
}

static void
gst_openexr_dec_init (GstOpenEXRDec * self)
{
  GstVideoDecoder *decoder = (GstVideoDecoder *) self;

  gst_video_decoder_set_packetized (decoder, FALSE);
  gst_video_decoder_set_use_default_pad_acceptcaps (GST_VIDEO_DECODER_CAST
      (self), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_DECODER_SINK_PAD (self));
}

static gboolean
gst_openexr_dec_start (GstVideoDecoder * decoder)
{
  GstOpenEXRDec *self = GST_OPENEXR_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Starting");

  return TRUE;
}

static gboolean
gst_openexr_dec_stop (GstVideoDecoder * video_decoder)
{
  GstOpenEXRDec *self = GST_OPENEXR_DEC (video_decoder);

  GST_DEBUG_OBJECT (self, "Stopping");

  if (self->output_state) {
    gst_video_codec_state_unref (self->output_state);
    self->output_state = NULL;
  }

  if (self->input_state) {
    gst_video_codec_state_unref (self->input_state);
    self->input_state = NULL;
  }

  GST_DEBUG_OBJECT (self, "Stopped");

  return TRUE;
}

static GstFlowReturn
gst_openexr_dec_parse (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, GstAdapter * adapter, gboolean at_eos)
{
  guint8 data[8];
  gsize size, parsed_size;
  guint32 magic, flags;
  gssize offset;

  size = gst_adapter_available (adapter);

  parsed_size = gst_video_decoder_get_pending_frame_size (decoder);

  GST_DEBUG_OBJECT (decoder, "Parsing OpenEXR image data %" G_GSIZE_FORMAT,
      size);

  if (parsed_size == 0 && size < 8)
    goto need_more_data;

  /* If we did not parse anything yet, check if the frame starts with the header */
  if (parsed_size == 0) {
    gst_adapter_copy (adapter, data, 0, 8);

    magic = GST_READ_UINT32_LE (data);
    flags = GST_READ_UINT32_LE (data + 4);
    if (magic != 0x01312f76 || ((flags & 0xff) != 1 && (flags & 0xff) != 2) || ((flags & 0x200) && (flags & 0x1800))) {
      offset = gst_adapter_masked_scan_uint32_peek (adapter, 0xffffffff, 0x762f3101, 0, size, NULL);
      if (offset == -1) {
        gst_adapter_flush (adapter, size - 4);
        goto need_more_data;
      }

      /* come back into this function after flushing some data */
      gst_adapter_flush (adapter, offset);
      goto need_more_data;
    }
  } else {
  }

  /* valid header, now let's try to find the next one unless we're EOS */
  if (!at_eos) {
    gboolean found = FALSE;

    while (!found) {
      offset = gst_adapter_masked_scan_uint32_peek (adapter, 0xffffffff, 0x762f3101, 8, size - 8 - 4, NULL);
      if (offset == -1) {        
        gst_video_decoder_add_to_frame (decoder, size - 7);
        goto need_more_data;
      }

      gst_adapter_copy (adapter, data, offset, 8);
      magic = GST_READ_UINT32_LE (data);
      flags = GST_READ_UINT32_LE (data + 4);
      if (magic == 0x01312f76 && ((flags & 0xff) == 1 || (flags & 0xff) == 2) && (!(flags & 0x200) || !(flags & 0x1800)))
        found = TRUE;
    }
    size = offset;
  }

  GST_DEBUG_OBJECT (decoder, "Have complete image of size %" G_GSSIZE_FORMAT, size + parsed_size);

  gst_video_decoder_add_to_frame (decoder, size);

  return gst_video_decoder_have_frame (decoder);

need_more_data:
  GST_DEBUG_OBJECT (decoder, "Need more data");
  return GST_VIDEO_DECODER_FLOW_NEED_DATA;
}

static gboolean
gst_openexr_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstOpenEXRDec *self = GST_OPENEXR_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Setting format: %" GST_PTR_FORMAT, state->caps);

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = gst_video_codec_state_ref (state);

  return TRUE;
}

static GstFlowReturn
gst_openexr_dec_negotiate (GstOpenEXRDec * self, RgbaInputFile * file)
{
  GstVideoFormat format;
  gint width, height;
  gfloat par;

  /* TODO: Use displayWindow here and also support output of ARGB_F16 */
  format = GST_VIDEO_FORMAT_ARGB64;
  Box2i dw = file->dataWindow ();
  width = dw.max.x - dw.min.x + 1;
  height = dw.max.y - dw.min.y + 1;
  par = file->pixelAspectRatio ();

  if (!self->output_state ||
      self->output_state->info.finfo->format != format ||
      self->output_state->info.width != width ||
      self->output_state->info.height != height) {
    if (self->output_state)
      gst_video_codec_state_unref (self->output_state);
    self->output_state =
        gst_video_decoder_set_output_state (GST_VIDEO_DECODER (self), format,
        width, height, self->input_state);

    GST_DEBUG_OBJECT (self, "Have image of size %dx%d (par %f)", width, height, par);
    gst_util_double_to_fraction (par, &self->output_state->info.par_n, &self->output_state->info.par_d);

    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self)))
      return GST_FLOW_NOT_NEGOTIATED;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_openexr_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstOpenEXRDec *self = GST_OPENEXR_DEC (decoder);
  GstFlowReturn ret = GST_FLOW_OK;
  gint64 deadline;
  GstMapInfo map;
  GstVideoFrame vframe;

  GST_DEBUG_OBJECT (self, "Handling frame");

  deadline = gst_video_decoder_get_max_decode_time (decoder, frame);
  if (deadline < 0) {
    GST_LOG_OBJECT (self, "Dropping too late frame: deadline %" G_GINT64_FORMAT,
        deadline);
    ret = gst_video_decoder_drop_frame (decoder, frame);
    return ret;
  }

  if (!gst_buffer_map (frame->input_buffer, &map, GST_MAP_READ)) {
    gst_video_codec_frame_unref (frame);

    GST_ELEMENT_ERROR (self, CORE, FAILED,
        ("Failed to map input buffer"), (NULL));
    return GST_FLOW_ERROR;
  }

  /* Now read the file and catch any exceptions */
  MemIStream *istr;
  RgbaInputFile *file;
  try {
    istr =
        new
        MemIStream (gst_pad_get_stream_id (GST_VIDEO_DECODER_SINK_PAD
            (decoder)), map.data, map.size);
  }
  catch (Iex::BaseExc& e) {
    gst_video_codec_frame_unref (frame);

    GST_ELEMENT_ERROR (self, CORE, FAILED,
        ("Failed to create input stream"), (NULL));
    return GST_FLOW_ERROR;
  }
  try {
    file = new RgbaInputFile (*istr);
  }
  catch (Iex::BaseExc& e) {
    delete istr;
    gst_video_codec_frame_unref (frame);

    GST_ELEMENT_ERROR (self, CORE, FAILED,
        ("Failed to read OpenEXR stream"), (NULL));
    return GST_FLOW_ERROR;
  }

  ret = gst_openexr_dec_negotiate (self, file);
  if (ret != GST_FLOW_OK) {
    delete file;
    delete istr;
    gst_buffer_unmap (frame->input_buffer, &map);
    gst_video_codec_frame_unref (frame);

    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION,
        ("Failed to negotiate"), (NULL));
    return ret;
  }

  ret = gst_video_decoder_allocate_output_frame (decoder, frame);
  if (ret != GST_FLOW_OK) {
    delete file;
    delete istr;
    gst_buffer_unmap (frame->input_buffer, &map);
    gst_video_codec_frame_unref (frame);

    GST_ELEMENT_ERROR (self, CORE, FAILED,
        ("Failed to allocate output buffer"), (NULL));
    return ret;
  }

  if (!gst_video_frame_map (&vframe, &self->output_state->info,
          frame->output_buffer, GST_MAP_WRITE)) {
    delete file;
    delete istr;
    gst_buffer_unmap (frame->input_buffer, &map);
    gst_video_codec_frame_unref (frame);

    GST_ELEMENT_ERROR (self, CORE, FAILED,
        ("Failed to map output buffer"), (NULL));
    return GST_FLOW_ERROR;
  }

  /* Decode the file */
  Box2i dw = file->dataWindow ();
  int width = dw.max.x - dw.min.x + 1;
  int height = dw.max.y - dw.min.y + 1;
  Rgba *fb = new Rgba[width * height];

  try {
    file->setFrameBuffer (fb - dw.min.x - dw.min.y * width, 1, width);
    file->readPixels (dw.min.y, dw.max.y);
  } catch (Iex::BaseExc& e) {
    delete[](fb);
    delete file;
    delete istr;
    gst_buffer_unmap (frame->input_buffer, &map);
    gst_video_frame_unmap (&vframe);

    GST_ELEMENT_ERROR (self, CORE, FAILED, ("Failed to read pixels"), (NULL));
    return GST_FLOW_ERROR;
  }

  /* And convert from ARGB64_F16 to ARGB64 */
  gint i, j;
  guint16 *dest = (guint16 *) GST_VIDEO_FRAME_PLANE_DATA (&vframe, 0);
  guint dstride = GST_VIDEO_FRAME_PLANE_STRIDE (&vframe, 0);
  Rgba *ptr = fb;

  /* TODO: Use displayWindow here and also support output of ARGB_F16
   * and add a conversion filter element that can change exposure and
   * other things */
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      dest[4 * j + 0] = CLAMP (((float) ptr->a) * 65536, 0, 65535);
      dest[4 * j + 1] = CLAMP (((float) ptr->r) * 65536, 0, 65535);
      dest[4 * j + 2] = CLAMP (((float) ptr->g) * 65536, 0, 65535);
      dest[4 * j + 3] = CLAMP (((float) ptr->b) * 65536, 0, 65535);
      ptr++;
    }
    dest += dstride / 2;
  }

  delete[](fb);
  delete file;
  delete istr;
  gst_buffer_unmap (frame->input_buffer, &map);
  gst_video_frame_unmap (&vframe);

  ret = gst_video_decoder_finish_frame (decoder, frame);

  return ret;
}

static gboolean
gst_openexr_dec_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstBufferPool *pool;
  GstStructure *config;

  if (!GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation (decoder,
          query))
    return FALSE;

  g_assert (gst_query_get_n_allocation_pools (query) > 0);
  gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, NULL, NULL);
  g_assert (pool != NULL);

  config = gst_buffer_pool_get_config (pool);
  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }
  gst_buffer_pool_set_config (pool, config);
  gst_object_unref (pool);

  return TRUE;
}
