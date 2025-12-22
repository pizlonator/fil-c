/* GStreamer
 * Copyright (C) 2021 Sebastian Dröge <sebastian@centricular.com>
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

#include <ajabase/common/types.h>
#include <ajabase/system/memory.h>
#include <ajabase/system/thread.h>
#include <ajantv2/includes/ntv2card.h>
#include <ajantv2/includes/ntv2devicefeatures.h>
#include <ajantv2/includes/ntv2devicescanner.h>
#include <ajantv2/includes/ntv2enums.h>
#include <ajantv2/includes/ntv2signalrouter.h>
#include <gst/base/base.h>
#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

typedef struct {
  GstMeta meta;

  GstBuffer *buffer;
} GstAjaAudioMeta;

G_GNUC_INTERNAL
GType gst_aja_audio_meta_api_get_type(void);
#define GST_AJA_AUDIO_META_API_TYPE (gst_aja_audio_meta_api_get_type())

G_GNUC_INTERNAL
const GstMetaInfo *gst_aja_audio_meta_get_info(void);
#define GST_AJA_AUDIO_META_INFO (gst_aja_audio_meta_get_info())

#define gst_buffer_get_aja_audio_meta(b) \
  ((GstAjaAudioMeta *)gst_buffer_get_meta((b), GST_AJA_AUDIO_META_API_TYPE))

G_GNUC_INTERNAL
GstAjaAudioMeta *gst_buffer_add_aja_audio_meta(GstBuffer *buffer,
                                               GstBuffer *audio_buffer);

typedef struct {
  CNTV2Card *device;
  int fd;
} GstAjaNtv2Device;

G_GNUC_INTERNAL
GstAjaNtv2Device *gst_aja_ntv2_device_obtain(const gchar *device_identifier);
G_GNUC_INTERNAL
GstAjaNtv2Device *gst_aja_ntv2_device_ref(GstAjaNtv2Device *device);
G_GNUC_INTERNAL
void gst_aja_ntv2_device_unref(GstAjaNtv2Device *device);

G_GNUC_INTERNAL
gint gst_aja_ntv2_device_find_unallocated_frames(GstAjaNtv2Device *device,
                                                 NTV2Channel channel,
                                                 guint frame_count);

#define GST_AJA_ALLOCATOR_MEMTYPE "aja"

#define GST_TYPE_AJA_ALLOCATOR (gst_aja_allocator_get_type())
#define GST_AJA_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AJA_ALLOCATOR, GstAjaAllocator))
#define GST_AJA_ALLOCATOR_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AJA_ALLOCATOR, \
                           GstAjaAllocatorClass))
#define GST_IS_Aja_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AJA_ALLOCATOR))
#define GST_IS_Aja_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AJA_ALLOCATOR))
#define GST_AJA_ALLOCATOR_CAST(obj) ((GstAjaAllocator *)(obj))

typedef struct _GstAjaAllocator GstAjaAllocator;
typedef struct _GstAjaAllocatorClass GstAjaAllocatorClass;

struct _GstAjaAllocator {
  GstAllocator allocator;

  GstAjaNtv2Device *device;
  GstQueueArray *freed_mems;
};

struct _GstAjaAllocatorClass {
  GstAllocatorClass parent_class;
};

G_GNUC_INTERNAL
GType gst_aja_allocator_get_type(void);
G_GNUC_INTERNAL
GstAllocator *gst_aja_allocator_new(GstAjaNtv2Device *device);

typedef enum {
  GST_AJA_AUDIO_SYSTEM_AUTO,
  GST_AJA_AUDIO_SYSTEM_1,
  GST_AJA_AUDIO_SYSTEM_2,
  GST_AJA_AUDIO_SYSTEM_3,
  GST_AJA_AUDIO_SYSTEM_4,
  GST_AJA_AUDIO_SYSTEM_5,
  GST_AJA_AUDIO_SYSTEM_6,
  GST_AJA_AUDIO_SYSTEM_7,
  GST_AJA_AUDIO_SYSTEM_8,
} GstAjaAudioSystem;

#define GST_TYPE_AJA_AUDIO_SYSTEM (gst_aja_audio_system_get_type())
G_GNUC_INTERNAL
GType gst_aja_audio_system_get_type(void);

typedef enum {
  GST_AJA_OUTPUT_DESTINATION_AUTO,
  GST_AJA_OUTPUT_DESTINATION_ANALOG,
  GST_AJA_OUTPUT_DESTINATION_SDI1,
  GST_AJA_OUTPUT_DESTINATION_SDI2,
  GST_AJA_OUTPUT_DESTINATION_SDI3,
  GST_AJA_OUTPUT_DESTINATION_SDI4,
  GST_AJA_OUTPUT_DESTINATION_SDI5,
  GST_AJA_OUTPUT_DESTINATION_SDI6,
  GST_AJA_OUTPUT_DESTINATION_SDI7,
  GST_AJA_OUTPUT_DESTINATION_SDI8,
  GST_AJA_OUTPUT_DESTINATION_HDMI,
} GstAjaOutputDestination;

#define GST_TYPE_AJA_OUTPUT_DESTINATION (gst_aja_output_destination_get_type())
G_GNUC_INTERNAL
GType gst_aja_output_destination_get_type(void);

typedef enum {
  GST_AJA_REFERENCE_SOURCE_AUTO,
  GST_AJA_REFERENCE_SOURCE_FREERUN,
  GST_AJA_REFERENCE_SOURCE_EXTERNAL,
  GST_AJA_REFERENCE_SOURCE_INPUT_1,
  GST_AJA_REFERENCE_SOURCE_INPUT_2,
  GST_AJA_REFERENCE_SOURCE_INPUT_3,
  GST_AJA_REFERENCE_SOURCE_INPUT_4,
  GST_AJA_REFERENCE_SOURCE_INPUT_5,
  GST_AJA_REFERENCE_SOURCE_INPUT_6,
  GST_AJA_REFERENCE_SOURCE_INPUT_7,
  GST_AJA_REFERENCE_SOURCE_INPUT_8,
} GstAjaReferenceSource;

#define GST_TYPE_AJA_REFERENCE_SOURCE (gst_aja_reference_source_get_type())
G_GNUC_INTERNAL
GType gst_aja_reference_source_get_type(void);

typedef enum {
  GST_AJA_INPUT_SOURCE_AUTO,
  GST_AJA_INPUT_SOURCE_ANALOG1,
  GST_AJA_INPUT_SOURCE_HDMI1,
  GST_AJA_INPUT_SOURCE_HDMI2,
  GST_AJA_INPUT_SOURCE_HDMI3,
  GST_AJA_INPUT_SOURCE_HDMI4,
  GST_AJA_INPUT_SOURCE_SDI1,
  GST_AJA_INPUT_SOURCE_SDI2,
  GST_AJA_INPUT_SOURCE_SDI3,
  GST_AJA_INPUT_SOURCE_SDI4,
  GST_AJA_INPUT_SOURCE_SDI5,
  GST_AJA_INPUT_SOURCE_SDI6,
  GST_AJA_INPUT_SOURCE_SDI7,
  GST_AJA_INPUT_SOURCE_SDI8,
} GstAjaInputSource;

#define GST_TYPE_AJA_INPUT_SOURCE (gst_aja_input_source_get_type())
G_GNUC_INTERNAL
GType gst_aja_input_source_get_type(void);

typedef enum {
  GST_AJA_SDI_MODE_SINGLE_LINK,
  GST_AJA_SDI_MODE_QUAD_LINK_SQD,
  GST_AJA_SDI_MODE_QUAD_LINK_TSI,
} GstAjaSdiMode;

#define GST_TYPE_AJA_SDI_MODE (gst_aja_sdi_mode_get_type())
G_GNUC_INTERNAL
GType gst_aja_sdi_mode_get_type(void);

typedef enum {
  GST_AJA_VIDEO_FORMAT_INVALID = -1,
  GST_AJA_VIDEO_FORMAT_AUTO,
  GST_AJA_VIDEO_FORMAT_1080i_5000,
  GST_AJA_VIDEO_FORMAT_1080i_5994,
  GST_AJA_VIDEO_FORMAT_1080i_6000,
  GST_AJA_VIDEO_FORMAT_720p_5994,
  GST_AJA_VIDEO_FORMAT_720p_6000,
  GST_AJA_VIDEO_FORMAT_1080psf_2398,
  GST_AJA_VIDEO_FORMAT_1080psf_2400,
  GST_AJA_VIDEO_FORMAT_1080p_2997,
  GST_AJA_VIDEO_FORMAT_1080p_3000,
  GST_AJA_VIDEO_FORMAT_1080p_2500,
  GST_AJA_VIDEO_FORMAT_1080p_2398,
  GST_AJA_VIDEO_FORMAT_1080p_2400,
  GST_AJA_VIDEO_FORMAT_720p_5000,
  GST_AJA_VIDEO_FORMAT_1080p_5000_A,
  GST_AJA_VIDEO_FORMAT_1080p_5994_A,
  GST_AJA_VIDEO_FORMAT_1080p_6000_A,
  GST_AJA_VIDEO_FORMAT_720p_2398,
  GST_AJA_VIDEO_FORMAT_720p_2500,
  GST_AJA_VIDEO_FORMAT_1080psf_2500_2,
  GST_AJA_VIDEO_FORMAT_1080psf_2997_2,
  GST_AJA_VIDEO_FORMAT_1080psf_3000_2,
  GST_AJA_VIDEO_FORMAT_625_5000,
  GST_AJA_VIDEO_FORMAT_525_5994,
  GST_AJA_VIDEO_FORMAT_525_2398,
  GST_AJA_VIDEO_FORMAT_525_2400,
  GST_AJA_VIDEO_FORMAT_1080p_DCI_2398,
  GST_AJA_VIDEO_FORMAT_1080p_DCI_2400,
  GST_AJA_VIDEO_FORMAT_1080p_DCI_2500,
  GST_AJA_VIDEO_FORMAT_1080p_DCI_2997,
  GST_AJA_VIDEO_FORMAT_1080p_DCI_3000,
  GST_AJA_VIDEO_FORMAT_1080p_DCI_5000_A,
  GST_AJA_VIDEO_FORMAT_1080p_DCI_5994_A,
  GST_AJA_VIDEO_FORMAT_1080p_DCI_6000_A,
  GST_AJA_VIDEO_FORMAT_2160p_2398,
  GST_AJA_VIDEO_FORMAT_2160p_2400,
  GST_AJA_VIDEO_FORMAT_2160p_2500,
  GST_AJA_VIDEO_FORMAT_2160p_2997,
  GST_AJA_VIDEO_FORMAT_2160p_3000,
  GST_AJA_VIDEO_FORMAT_2160p_5000,
  GST_AJA_VIDEO_FORMAT_2160p_5994,
  GST_AJA_VIDEO_FORMAT_2160p_6000,
  GST_AJA_VIDEO_FORMAT_2160p_DCI_2398,
  GST_AJA_VIDEO_FORMAT_2160p_DCI_2400,
  GST_AJA_VIDEO_FORMAT_2160p_DCI_2500,
  GST_AJA_VIDEO_FORMAT_2160p_DCI_2997,
  GST_AJA_VIDEO_FORMAT_2160p_DCI_3000,
  GST_AJA_VIDEO_FORMAT_2160p_DCI_5000,
  GST_AJA_VIDEO_FORMAT_2160p_DCI_5994,
  GST_AJA_VIDEO_FORMAT_2160p_DCI_6000,
  GST_AJA_VIDEO_FORMAT_4320p_2398,
  GST_AJA_VIDEO_FORMAT_4320p_2400,
  GST_AJA_VIDEO_FORMAT_4320p_2500,
  GST_AJA_VIDEO_FORMAT_4320p_2997,
  GST_AJA_VIDEO_FORMAT_4320p_3000,
  GST_AJA_VIDEO_FORMAT_4320p_5000,
  GST_AJA_VIDEO_FORMAT_4320p_5994,
  GST_AJA_VIDEO_FORMAT_4320p_6000,
  GST_AJA_VIDEO_FORMAT_4320p_DCI_2398,
  GST_AJA_VIDEO_FORMAT_4320p_DCI_2400,
  GST_AJA_VIDEO_FORMAT_4320p_DCI_2500,
  GST_AJA_VIDEO_FORMAT_4320p_DCI_2997,
  GST_AJA_VIDEO_FORMAT_4320p_DCI_3000,
  GST_AJA_VIDEO_FORMAT_4320p_DCI_5000,
  GST_AJA_VIDEO_FORMAT_4320p_DCI_5994,
  GST_AJA_VIDEO_FORMAT_4320p_DCI_6000,
} GstAjaVideoFormat;

#define GST_TYPE_AJA_VIDEO_FORMAT (gst_aja_video_format_get_type())
G_GNUC_INTERNAL
GType gst_aja_video_format_get_type(void);

typedef enum {
  GST_AJA_AUDIO_SOURCE_EMBEDDED,
  GST_AJA_AUDIO_SOURCE_AES,
  GST_AJA_AUDIO_SOURCE_ANALOG,
  GST_AJA_AUDIO_SOURCE_HDMI,
  GST_AJA_AUDIO_SOURCE_MIC,
} GstAjaAudioSource;

#define GST_TYPE_AJA_AUDIO_SOURCE (gst_aja_audio_source_get_type())
G_GNUC_INTERNAL
GType gst_aja_audio_source_get_type(void);

typedef enum {
  GST_AJA_EMBEDDED_AUDIO_INPUT_AUTO,
  GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO1,
  GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO2,
  GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO3,
  GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO4,
  GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO5,
  GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO6,
  GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO7,
  GST_AJA_EMBEDDED_AUDIO_INPUT_VIDEO8,
} GstAjaEmbeddedAudioInput;

#define GST_TYPE_AJA_EMBEDDED_AUDIO_INPUT \
  (gst_aja_embedded_audio_input_get_type())
G_GNUC_INTERNAL
GType gst_aja_embedded_audio_input_get_type(void);

typedef enum {
  GST_AJA_TIMECODE_INDEX_VITC,
  GST_AJA_TIMECODE_INDEX_ATC_LTC,
  GST_AJA_TIMECODE_INDEX_LTC1,
  GST_AJA_TIMECODE_INDEX_LTC2,
} GstAjaTimecodeIndex;

#define GST_TYPE_AJA_TIMECODE_INDEX (gst_aja_timecode_index_get_type())
G_GNUC_INTERNAL
GType gst_aja_timecode_index_get_type(void);

typedef enum {
  GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_AND_CEA608,
  GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_OR_CEA608,
  GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA608_OR_CEA708,
  GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA708_ONLY,
  GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_CEA608_ONLY,
  GST_AJA_CLOSED_CAPTION_CAPTURE_MODE_NONE,
} GstAjaClosedCaptionCaptureMode;

#define GST_TYPE_AJA_CLOSED_CAPTION_CAPTURE_MODE \
  (gst_aja_closed_caption_capture_mode_get_type())
G_GNUC_INTERNAL
GType gst_aja_closed_caption_capture_mode_get_type(void);

G_GNUC_INTERNAL
void gst_aja_common_init(void);

G_END_DECLS

class GstAjaNtv2DeviceLocker {
 public:
  GstAjaNtv2DeviceLocker(GstAjaNtv2Device *device);
  ~GstAjaNtv2DeviceLocker();
 private:
  GstAjaNtv2Device *_device;
};

G_GNUC_INTERNAL
GstCaps *gst_ntv2_supported_caps(NTV2DeviceID device_id);

G_GNUC_INTERNAL
GstCaps *gst_ntv2_video_format_to_caps(NTV2VideoFormat format);
G_GNUC_INTERNAL
bool gst_video_info_from_ntv2_video_format(GstVideoInfo *info,
                                           NTV2VideoFormat format);
G_GNUC_INTERNAL
NTV2VideoFormat gst_ntv2_video_format_from_caps(const GstCaps *caps, bool quad);

G_GNUC_INTERNAL
GstCaps *gst_aja_video_format_to_caps(GstAjaVideoFormat format);
G_GNUC_INTERNAL
bool gst_video_info_from_aja_video_format(GstVideoInfo *info,
                                          GstAjaVideoFormat format);
G_GNUC_INTERNAL
GstAjaVideoFormat gst_aja_video_format_from_caps(const GstCaps *caps);

G_GNUC_INTERNAL
GstAjaVideoFormat gst_aja_video_format_from_ntv2_format(NTV2VideoFormat format);
G_GNUC_INTERNAL
NTV2VideoFormat gst_ntv2_video_format_from_aja_format(GstAjaVideoFormat format,
                                                      bool quad);

G_GNUC_INTERNAL
bool gst_ntv2_video_format_is_quad(NTV2VideoFormat format);
