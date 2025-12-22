/*
 * GStreamer
 * Copyright (C) 2023 Igalia, S.L.
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

#pragma once

#include <gst/vulkan/gstvkqueue.h>
#include <gst/vulkan/gstvkvideoutils.h>

G_BEGIN_DECLS

#define GST_TYPE_VULKAN_DECODER         (gst_vulkan_decoder_get_type())
#define GST_VULKAN_DECODER(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_VULKAN_DECODER, GstVulkanDecoder))
#define GST_VULKAN_DECODER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GST_TYPE_VULKAN_DECODER, GstVulkanDecoderClass))
#define GST_IS_VULKAN_DECODER(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_VULKAN_DECODER))
#define GST_IS_VULKAN_DECODER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_VULKAN_DECODER))
#define GST_VULKAN_DECODER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_VULKAN_DECODER, GstVulkanDecoderClass))
GST_VULKAN_API
GType gst_vulkan_decoder_get_type       (void);

/**
 * GstVulkanDecoderPicture:
 * @out: output buffer
 * @dpb: DPB representation of @out if needed by driver
 * @img_view_ref: image view for reference
 * @img_view_out: image view for output
 * @slice_offs: array of offsets of each uploaded slice
 * @refs: references required to decode current pictures
 *
 * It contains the whole state for decoding a single picture.
 *
 * Since: 1.24
 */
struct _GstVulkanDecoderPicture
{
  GstBuffer *out;
  GstBuffer *dpb; /* only used for out-of-place decoding */

  GstVulkanImageView *img_view_ref; /* Image representation view (reference) */
  GstVulkanImageView *img_view_out; /* Image representation view (output-only) */

  GArray *slice_offs;

  /* Picture refs. H264 has the maximum number of refs (36) of any supported
   * codec. */
  GstVulkanDecoderPicture *refs[36];

  /*< private >*/
  VkVideoPictureResourceInfoKHR pics_res[36];
  VkVideoReferenceSlotInfoKHR slots[36];

  /* Current picture */
  VkVideoPictureResourceInfoKHR pic_res;
  VkVideoReferenceSlotInfoKHR slot;

  /* Main decoding struct */
  VkVideoDecodeInfoKHR decode_info;
};

/**
 * GstVulkanDecoder:
 * @parent: the parent #GstObject
 * @queue: the #GstVulkanQueue to command buffers will be allocated from
 * @codec: the configured video codec operation
 * @profile: the configured #GstVulkanVideoProfile
 * @input_buffer: the buffer to upload the bitstream to decode
 * @dedicated_dpb: if decoder needs a dedicated DPB
 * @layered_dpb: if decoder's dedicated DPB has to be a layered image
 *
 * Since: 1.24
 **/
struct _GstVulkanDecoder
{
  GstObject parent;

  GstVulkanQueue *queue;

  guint codec;
  GstVulkanVideoProfile profile;
  GstBuffer *input_buffer;
  GstBuffer *layered_buffer;

  gboolean dedicated_dpb;
  gboolean layered_dpb;

  /*< private >*/
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanDecoderClass:
 * @parent_class: the parent #GstObjectClass
 *
 * Since: 1.24
 */
struct _GstVulkanDecoderClass
{
  GstObjectClass parent;
  /*< private >*/
  gpointer _reserved        [GST_PADDING];
};

/**
 * GstVulkanDecoderParameters:
 *
 * Codec specific parameters.
 *
 * Since: 1.24
 */
union _GstVulkanDecoderParameters
{
  /*< private >*/
  VkVideoDecodeH264SessionParametersCreateInfoKHR h264;
  VkVideoDecodeH265SessionParametersCreateInfoKHR h265;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstVulkanDecoder, gst_object_unref)

GST_VULKAN_API
gboolean                gst_vulkan_decoder_start                (GstVulkanDecoder * self,
                                                                 GstVulkanVideoProfile * profile,
                                                                 GError ** error);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_stop                 (GstVulkanDecoder * self);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_update_video_session_parameters
                                                                (GstVulkanDecoder * self,
                                                                 GstVulkanDecoderParameters * params,
                                                                 GError ** error);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_create_dpb_pool      (GstVulkanDecoder * self,
                                                                 GstCaps * caps);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_flush                (GstVulkanDecoder * self,
                                                                 GError ** error);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_decode               (GstVulkanDecoder * self,
                                                                 GstVulkanDecoderPicture * pic,
                                                                 GError ** error);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_is_started           (GstVulkanDecoder * self);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_caps                 (GstVulkanDecoder * self,
                                                                 GstVulkanVideoCapabilities * caps);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_out_format           (GstVulkanDecoder * self,
                                                                 VkVideoFormatPropertiesKHR * format);
GST_VULKAN_API
GstCaps *               gst_vulkan_decoder_profile_caps         (GstVulkanDecoder * self);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_update_ycbcr_sampler (GstVulkanDecoder * self,
                                                                 VkSamplerYcbcrRange range,
                                                                 VkChromaLocation xloc,
                                                                 VkChromaLocation yloc,
                                                                 GError ** error);

GST_VULKAN_API
GstVulkanImageView *    gst_vulkan_decoder_picture_create_view (GstVulkanDecoder * self,
                                                                GstBuffer * buf,
                                                                gboolean is_out);
GST_VULKAN_API
gboolean                gst_vulkan_decoder_picture_init         (GstVulkanDecoder * self,
                                                                 GstVulkanDecoderPicture * pic,
                                                                 GstBuffer * out);
GST_VULKAN_API
void                    gst_vulkan_decoder_picture_release      (GstVulkanDecoderPicture * pic);

GST_VULKAN_API
gboolean                gst_vulkan_decoder_append_slice         (GstVulkanDecoder * self,
                                                                 GstVulkanDecoderPicture * pic,
                                                                 const guint8 * data,
                                                                 size_t size,
                                                                 gboolean add_startcode);

GST_VULKAN_API
gboolean               gst_vulkan_decoder_wait                  (GstVulkanDecoder * self);

G_END_DECLS
