/*
 * uvc_h264.h - Definitions of the UVC H.264 Payload specification Version 1.0
 *
 *             Copyright (c) 2011 USB Implementers Forum, Inc.
 *
 * Modification into glib-like header by :
 * Copyright (C) 2012 Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _UVC_H264_H_
#define _UVC_H264_H_

/* Header File for the little-endian platform */

#include <glib.h>
#include <glib-object.h>

#include <gst/video/video.h>
#include <linux/uvcvideo.h>
#include <linux/usb/video.h>
#include <sys/ioctl.h>
#include <string.h>

#include <gudev/gudev.h>
#include <libusb.h>

#include <gst/gst.h>
#include <libusb.h>

#define USB_VIDEO_CONTROL		1
#define USB_VIDEO_CONTROL_INTERFACE	0x24
#define USB_VIDEO_CONTROL_XU_TYPE	0x06
#ifndef LIBUSB_CLASS_VIDEO
#define LIBUSB_CLASS_VIDEO 0x0e
#endif

/* bmHints defines */

#define UVC_H264_BMHINTS_RESOLUTION        (0x0001)
#define UVC_H264_BMHINTS_PROFILE           (0x0002)
#define UVC_H264_BMHINTS_RATECONTROL       (0x0004)
#define UVC_H264_BMHINTS_USAGE             (0x0008)
#define UVC_H264_BMHINTS_SLICEMODE         (0x0010)
#define UVC_H264_BMHINTS_SLICEUNITS        (0x0020)
#define UVC_H264_BMHINTS_MVCVIEW           (0x0040)
#define UVC_H264_BMHINTS_TEMPORAL          (0x0080)
#define UVC_H264_BMHINTS_SNR               (0x0100)
#define UVC_H264_BMHINTS_SPATIAL           (0x0200)
#define UVC_H264_BMHINTS_SPATIAL_RATIO     (0x0400)
#define UVC_H264_BMHINTS_FRAME_INTERVAL    (0x0800)
#define UVC_H264_BMHINTS_LEAKY_BKT_SIZE    (0x1000)
#define UVC_H264_BMHINTS_BITRATE           (0x2000)
#define UVC_H264_BMHINTS_ENTROPY           (0x4000)
#define UVC_H264_BMHINTS_IFRAMEPERIOD      (0x8000)


#define UVC_H264_QP_STEPS_I_FRAME_TYPE     (0x01)
#define UVC_H264_QP_STEPS_P_FRAME_TYPE     (0x02)
#define UVC_H264_QP_STEPS_B_FRAME_TYPE     (0x04)
#define UVC_H264_QP_STEPS_ALL_FRAME_TYPES  (UVC_H264_QP_STEPS_I_FRAME_TYPE | \
      UVC_H264_QP_STEPS_P_FRAME_TYPE | UVC_H264_QP_STEPS_B_FRAME_TYPE)

/* wSliceMode defines */

typedef enum
{
  UVC_H264_SLICEMODE_IGNORED = 0x0000,
  UVC_H264_SLICEMODE_BITSPERSLICE = 0x0001,
  UVC_H264_SLICEMODE_MBSPERSLICE = 0x0002,
  UVC_H264_SLICEMODE_SLICEPERFRAME = 0x0003
} UvcH264SliceMode;

#define UVC_H264_SLICEMODE_TYPE (uvc_h264_slicemode_get_type())

GType uvc_h264_slicemode_get_type (void);

/* bUsageType defines */

typedef enum {
  UVC_H264_USAGETYPE_REALTIME = 0x01,
  UVC_H264_USAGETYPE_BROADCAST = 0x02,
  UVC_H264_USAGETYPE_STORAGE = 0x03,
  UVC_H264_USAGETYPE_UCCONFIG_0 = 0x04,
  UVC_H264_USAGETYPE_UCCONFIG_1 = 0x05,
  UVC_H264_USAGETYPE_UCCONFIG_2Q = 0x06,
  UVC_H264_USAGETYPE_UCCONFIG_2S = 0x07,
  UVC_H264_USAGETYPE_UCCONFIG_3 = 0x08,
} UvcH264UsageType;

#define UVC_H264_USAGETYPE_TYPE (uvc_h264_usagetype_get_type())

GType uvc_h264_usagetype_get_type (void);

/* bRateControlMode defines */

typedef enum {
  UVC_H264_RATECONTROL_CBR = 0x01,
  UVC_H264_RATECONTROL_VBR = 0x02,
  UVC_H264_RATECONTROL_CONST_QP = 0x03,
} UvcH264RateControl;

#define UVC_H264_RATECONTROL_FIXED_FRM_FLG (0x10)

#define UVC_H264_RATECONTROL_TYPE (uvc_h264_ratecontrol_get_type())

GType uvc_h264_ratecontrol_get_type (void);

/* bStreamFormat defines */

typedef enum {
  UVC_H264_STREAMFORMAT_ANNEXB = 0x00,
  UVC_H264_STREAMFORMAT_NAL = 0x01,
} UvcH264StreamFormat;

#define UVC_H264_STREAMFORMAT_TYPE (uvc_h264_streamformat_get_type())

GType uvc_h264_streamformat_get_type (void);

/* bEntropyCABAC defines */

typedef enum {
  UVC_H264_ENTROPY_CAVLC = 0x00,
  UVC_H264_ENTROPY_CABAC = 0x01,
} UvcH264Entropy;

#define UVC_H264_ENTROPY_TYPE (uvc_h264_entropy_get_type())

GType uvc_h264_entropy_get_type (void);

/* bProfile defines */
#define UVC_H264_PROFILE_CONSTRAINED_BASELINE 0x4240
#define UVC_H264_PROFILE_BASELINE 0x4200
#define UVC_H264_PROFILE_MAIN 0x4D00
#define UVC_H264_PROFILE_HIGH 0x6400

/* bTimingstamp defines */

#define UVC_H264_TIMESTAMP_SEI_DISABLE     (0x00)
#define UVC_H264_TIMESTAMP_SEI_ENABLE      (0x01)

/* bPreviewFlipped defines */

#define UVC_H264_PREFLIPPED_DISABLE        (0x00)
#define UVC_H264_PREFLIPPED_HORIZONTAL     (0x01)

/* wPicType defines */
#define UVC_H264_PICTYPE_I_FRAME              (0x00)
#define UVC_H264_PICTYPE_IDR                  (0x01)
#define UVC_H264_PICTYPE_IDR_WITH_PPS_SPS     (0x02)


/* wLayerID Macro */

/*                              wLayerID
  |------------+------------+------------+----------------+------------|
  |  Reserved  |  StreamID  | QualityID  |  DependencyID  | TemporalID |
  |  (3 bits)  |  (3 bits)  | (3 bits)   |  (4 bits)      | (3 bits)   |
  |------------+------------+------------+----------------+------------|
  |15        13|12        10|9          7|6              3|2          0|
  |------------+------------+------------+----------------+------------|
*/

#define xLayerID(stream_id, quality_id, dependency_id, temporal_id) \
  ((((stream_id) & 7) << 10) |                                      \
      (((quality_id) & 7) << 7) |                                   \
      (((dependency_id) & 15) << 3) |                               \
      ((temporal_id) & 7))

/* id extraction from wLayerID */

#define xStream_id(layer_id)      (((layer_id) >> 10) & 7)
#define xQuality_id(layer_id)     (((layer_id) >> 7) & 7)
#define xDependency_id(layer_id)  (((layer_id) >> 3) & 15)
#define xTemporal_id(layer_id)    ((layer_id)&7)

/* UVC H.264 control selectors */

typedef enum _uvcx_control_selector_t
{
	UVCX_VIDEO_CONFIG_PROBE			= 0x01,
	UVCX_VIDEO_CONFIG_COMMIT		= 0x02,
	UVCX_RATE_CONTROL_MODE			= 0x03,
	UVCX_TEMPORAL_SCALE_MODE		= 0x04,
	UVCX_SPATIAL_SCALE_MODE			= 0x05,
	UVCX_SNR_SCALE_MODE				= 0x06,
	UVCX_LTR_BUFFER_SIZE_CONTROL	= 0x07,
	UVCX_LTR_PICTURE_CONTROL		= 0x08,
	UVCX_PICTURE_TYPE_CONTROL		= 0x09,
	UVCX_VERSION					= 0x0A,
	UVCX_ENCODER_RESET				= 0x0B,
	UVCX_FRAMERATE_CONFIG			= 0x0C,
	UVCX_VIDEO_ADVANCE_CONFIG		= 0x0D,
	UVCX_BITRATE_LAYERS				= 0x0E,
	UVCX_QP_STEPS_LAYERS			= 0x0F,
} uvcx_control_selector_t;


typedef struct _uvcx_video_config_probe_commit_t
{
	guint32	dwFrameInterval;
	guint32	dwBitRate;
	guint16	bmHints;
	guint16	wConfigurationIndex;
	guint16	wWidth;
	guint16	wHeight;
	guint16	wSliceUnits;
	guint16	wSliceMode;
	guint16	wProfile;
	guint16	wIFramePeriod;
	guint16	wEstimatedVideoDelay;
	guint16	wEstimatedMaxConfigDelay;
	guint8	bUsageType;
	guint8	bRateControlMode;
	guint8	bTemporalScaleMode;
	guint8	bSpatialScaleMode;
	guint8	bSNRScaleMode;
	guint8	bStreamMuxOption;
	guint8	bStreamFormat;
	guint8	bEntropyCABAC;
	guint8	bTimestamp;
	guint8	bNumOfReorderFrames;
	guint8	bPreviewFlipped;
	guint8	bView;
	guint8	bReserved1;
	guint8	bReserved2;
	guint8	bStreamID;
	guint8	bSpatialLayerRatio;
	guint16	wLeakyBucketSize;
} __attribute__((packed)) uvcx_video_config_probe_commit_t;


typedef struct _uvcx_rate_control_mode_t
{
	guint16	wLayerID;
	guint8	bRateControlMode;
} __attribute__((packed)) uvcx_rate_control_mode_t;


typedef struct _uvcx_temporal_scale_mode_t
{
	guint16	wLayerID;
	guint8	bTemporalScaleMode;
} __attribute__((packed)) uvcx_temporal_scale_mode_t;


typedef struct _uvcx_spatial_scale_mode_t
{
	guint16	wLayerID;
	guint8	bSpatialScaleMode;
} __attribute__((packed)) uvcx_spatial_scale_mode_t;


typedef struct _uvcx_snr_scale_mode_t
{
	guint16	wLayerID;
	guint8	bSNRScaleMode;
	guint8	bMGSSublayerMode;
} __attribute__((packed)) uvcx_snr_scale_mode_t;


typedef struct _uvcx_ltr_buffer_size_control_t
{
	guint16	wLayerID;
	guint8	bLTRBufferSize;
	guint8	bLTREncoderControl;
} __attribute__((packed)) uvcx_ltr_buffer_size_control_t;

typedef struct _uvcx_ltr_picture_control
{
	guint16	wLayerID;
	guint8	bPutAtPositionInLTRBuffer;
	guint8	bEncodeUsingLTR;
} __attribute__((packed)) uvcx_ltr_picture_control;


typedef struct _uvcx_picture_type_control_t
{
	guint16	wLayerID;
	guint16	wPicType;
} __attribute__((packed)) uvcx_picture_type_control_t;


typedef struct _uvcx_version_t
{
	guint16	wVersion;
} __attribute__((packed)) uvcx_version_t;


typedef struct _uvcx_encoder_reset
{
	guint16	wLayerID;
} __attribute__((packed)) uvcx_encoder_reset;


typedef struct _uvcx_framerate_config_t
{
	guint16	wLayerID;
	guint32	dwFrameInterval;
} __attribute__((packed)) uvcx_framerate_config_t;


typedef struct _uvcx_video_advance_config_t
{
	guint16	wLayerID;
	guint32	dwMb_max;
	guint8	blevel_idc;
	guint8	bReserved;
} __attribute__((packed)) uvcx_video_advance_config_t;


typedef struct _uvcx_bitrate_layers_t
{
	guint16	wLayerID;
	guint32	dwPeakBitrate;
	guint32	dwAverageBitrate;
} __attribute__((packed)) uvcx_bitrate_layers_t;


typedef struct _uvcx_qp_steps_layers_t
{
	guint16	wLayerID;
	guint8	bFrameType;
	guint8	bMinQp;
	guint8	bMaxQp;
} __attribute__((packed)) uvcx_qp_steps_layers_t;

typedef struct
{
  int8_t bLength;
  int8_t bDescriptorType;
  int8_t bDescriptorSubType;
  int8_t bUnitID;
  uint8_t guidExtensionCode[16];
} __attribute__ ((__packed__)) xu_descriptor;

#define GUID_FORMAT "02X%02X%02X%02X-%02X%02X%02X%02X-"\
  "%02X%02X%02X%02X-%02X%02X%02X%02X"
#define GUID_ARGS(guid) guid[0], guid[1], guid[2], guid[3],       \
    guid[4], guid[5], guid[6], guid[7],                           \
    guid[8], guid[9], guid[10], guid[11],                         \
    guid[12], guid[13], guid[14], guid[15]

guint8 xu_get_id (GstObject *self, const gchar *devname, libusb_context **usb_ctx);

#ifdef _WIN32
// GUID of the UVC H.264 extension unit: {A29E7641-DE04-47E3-8B2B-F4341AFF003B}
DEFINE_GUID(GUID_UVCX_H264_XU, 0xA29E7641, 0xDE04, 0x47E3, 0x8B, 0x2B, 0xF4, 0x34, 0x1A, 0xFF, 0x00, 0x3B);
#else
#define GUID_UVCX_H264_XU                                               \
  {0x41, 0x76, 0x9e, 0xa2, 0x04, 0xde, 0xe3, 0x47, 0x8b, 0x2b, 0xF4, 0x34, 0x1A, 0xFF, 0x00, 0x3B}
#endif

#endif  /*_UVC_H264_H_*/
