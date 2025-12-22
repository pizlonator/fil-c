/* GStreamer
 *  Copyright (C) <2019> Aaron Boxer <aaron.boxer@collabora.com>
 *  Copyright (C) <2019> Collabora Ltd.
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

#ifndef __VIDEO_PARSE_UTILS_H__
#define __VIDEO_PARSE_UTILS_H__

#include <gst/gst.h>
#include <gst/base/gstbaseparse.h>
#include <gst/base/gstbytereader.h>
#include <gst/video/video-anc.h>

#define GST_VIDEO_BAR_MAX_BYTES 9

/* A53-4 Table 6.7 */
#define A53_USER_DATA_ID_GA94 0x47413934
#define A53_USER_DATA_ID_DTG1 0x44544731

/* custom id for SCTE 20 608 */
#define USER_DATA_ID_SCTE_20_CC 0xFFFFFFFE
/* custom id for DirecTV */
#define USER_DATA_ID_DIRECTV_CC 0xFFFFFFFF

/* A53-4 Table 6.9 */
#define A53_USER_DATA_TYPE_CODE_CC_DATA 0x03
#define A53_USER_DATA_TYPE_CODE_BAR_DATA 0x06
/* ANSI/SCTE 21 Additional EIA 608 Data
 * https://www.scte.org/documents/pdf/Standards/ANSISCTE212001R2006.pdf*/
#define A53_USER_DATA_TYPE_CODE_SCTE_21_EIA_608_CC_DATA 0x04

/* CEA-708 Table 2 */
#define CEA_708_PROCESS_CC_DATA_FLAG 0x40
#define CEA_708_PROCESS_EM_DATA_FLAG 0x80

/* country codes */
#define ITU_T_T35_COUNTRY_CODE_US			0xB5

/* provider codes */
#define ITU_T_T35_MANUFACTURER_US_ATSC     	0x31
#define ITU_T_T35_MANUFACTURER_US_DIRECTV  	0x2F

/*
 * GstVideoAFDAspectRatio:
 * @GST_VIDEO_AFD_ASPECT_RATIO_UNDEFINED: aspect ratio is undefined
 * @GST_VIDEO_AFD_ASPECT_RATIO_4_3: 4:3 aspect ratio
 * @GST_VIDEO_AFD_ASPECT_RATIO_16_9: 16:9 aspect ratio
 *
 * Enumeration of the different AFD aspect ratios (SMPTE ST2016-1 only)
 */
typedef enum {
  GST_VIDEO_AFD_ASPECT_RATIO_UNDEFINED,
  GST_VIDEO_AFD_ASPECT_RATIO_4_3,
  GST_VIDEO_AFD_ASPECT_RATIO_16_9
} GstVideoAFDAspectRatio;

/*
 * GstVideoParseUtilsField:
 * @GST_VIDEO_PARSE_UTILS_FIELD_1 progressive or field 1
 * @GST_VIDEO_PARSE_UTILS_FIELD_2 field 2
 *
 * Enumeration of fields
 */
typedef enum {
  GST_VIDEO_PARSE_UTILS_FIELD_1,
  GST_VIDEO_PARSE_UTILS_FIELD_2
} GstVideoParseUtilsField;

/*
 * GstVideoAFD:
 * @field: #GstVideoParseUtilsField for @afd
 * @aspect_ratio: #GstVideoAFDAspectRatio for frame
 * @spec: #GstVideoAFDSpec that applies to @afd
 * @afd: #GstVideoAFDValue AFD value
 *
 * Active Format Description (AFD)
 *
 * For details, see Table 6.14 Active Format in:
 *
 * ATSC Digital Television Standard:
 * Part 4 – MPEG-2 Video System Characteristics
 *
 * https://www.atsc.org/wp-content/uploads/2015/03/a_53-Part-4-2009.pdf
 *
 * and Active Format Description  in Complete list of AFD codes
 *
 * https://en.wikipedia.org/wiki/Active_Format_Description#Complete_list_of_AFD_codes
 *
 * and SMPTE ST2016-1
 */
 typedef struct {
   GstVideoParseUtilsField field;
   GstVideoAFDAspectRatio aspect_ratio;
   GstVideoAFDSpec spec;
   GstVideoAFDValue afd;
} GstVideoAFD;

/*
 * GstVideoBarData:
 * @field: GstVideoParseUtilsField for bar data
 * @is_letterbox: if true then bar data specifies letterbox, otherwise pillarbox
 * @bar_data: An array of size 2. if @is_letterbox is true, then two values specify
 * last line of a horizontal letterbox bar area at top of reconstructed frame
 * and first line of a horizontal letterbox bar area at bottom of reconstructed frame
 * otherwise, two values specify
 * last horizontal luminance sample of a vertical pillarbox  bar area at the left side
 * of the reconstructed frame, and first horizontal luminance sample of a vertical pillarbox
 * bar area at the right side of the  reconstructed frame
 *
 * Bar data should be included in video user data
 * whenever the rectangular picture area containing useful information
 * does not extend to the full  height or width of the coded frame
 * and AFD alone is insufficient to describe the extent of the image.
 *
 * Note: either vertical or horizontal bars are specified, but not both.
 *
 * For more details, see:
 *
 * https://www.atsc.org/wp-content/uploads/2015/03/a_53-Part-4-2009.pdf
 *
 * and SMPTE ST2016-1
 */
typedef struct {
  GstVideoParseUtilsField field;
  gboolean is_letterbox;
  guint bar_data[2];
} GstVideoBarData;

/*
 * GstVideoParseUserData
 *
 * Holds unparsed and parsed user data for closed captions, AFD and Bar data.
 */
typedef struct
{
  GstVideoParseUtilsField field;

  /* pending closed captions */
  guint8 closedcaptions[96];
  guint closedcaptions_size;
  GstVideoCaptionType closedcaptions_type;

  /* pending bar data */
  guint8 bar_data[GST_VIDEO_BAR_MAX_BYTES];
  guint bar_data_size;

  /* pending AFD data */
  guint8 afd;
  gboolean active_format_flag;
  GstVideoAFDSpec afd_spec;

} GstVideoParseUserData;

/*
 * GstVideoParseUserDataUnregistered
 *
 * Holds unparsed, unregistered user data.
 */
typedef struct
{
  /* item type: GstVideoUnregisteredMessage */
  GArray *messages;
} GstVideoParseUserDataUnregistered;

/*
 * GstVideoUnregisteredMessage
 *
 * Item type for the unregistered user data.
 */
typedef struct {
  guint8 uuid[16];
  guint8 *data;
  gsize size;
} GstVideoUnregisteredMessage;

G_BEGIN_DECLS

void gst_video_parse_user_data (GstElement            * elt,
                                GstVideoParseUserData * user_data,
                                GstByteReader         * br,
                                guint8                  field,
                                guint16                 provider_code);

void gst_video_push_user_data (GstElement            * elt,
                               GstVideoParseUserData * user_data,
                               GstBuffer             * buf);

void gst_video_clear_user_data (GstVideoParseUserData * user_data);


void gst_video_parse_user_data_unregistered (GstElement                        * elt,
                                             GstVideoParseUserDataUnregistered * user_data,
                                             GstByteReader                     * br,
                                             guint8                              uuid[16]);

void gst_video_push_user_data_unregistered (GstElement                        * elt,
                                            GstVideoParseUserDataUnregistered * user_data,
                                            GstBuffer                         * buf);

void gst_video_clear_user_data_unregistered (GstVideoParseUserDataUnregistered * user_data,
                                             gboolean                            free);


G_END_DECLS
#endif /* __VIDEO_PARSE_UTILS_H__ */
