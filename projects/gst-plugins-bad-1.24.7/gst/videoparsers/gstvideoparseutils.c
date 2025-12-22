/* GStreamer
 *  Copyright (2019) Collabora Ltd.
 *  Contact: Aaron Boxer <aaron.boxer@collabora.com>
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
#include <gst/base/base.h>
#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <gst/video/video-sei.h>
#include <gst/base/gstbitreader.h>
#include <gstvideoparseutils.h>

GST_DEBUG_CATEGORY_EXTERN (videoparseutils_debug);
#define GST_CAT_DEFAULT videoparseutils_debug

static gboolean gst_video_parse_utils_parse_bar (const guint8 * data,
    gsize size, GstVideoParseUtilsField field, GstVideoBarData * bar);

static gboolean gst_video_parse_utils_parse_afd (const guint8 data,
    GstVideoAFDSpec spec, GstVideoParseUtilsField field, GstVideoAFD * afd);

static void gst_video_clear_unregistered_message
    (GstVideoUnregisteredMessage * msg);


/*
 * gst_video_parse_user_data:
 * @elt: #GstElement that is parsing user data
 * @user_data: #GstVideoParseUserData struct to hold parsed closed caption, bar and AFD data
 * @br: #GstByteReader attached to buffer of user data
 * @field: 0 for progressive or field 1 and 1 for field 2
 * @provider_code: Currently, only (US) ATSC and DirecTV provider codes are supported
 *
 * Parse user data and store in @user_data
 */
void
gst_video_parse_user_data (GstElement * elt, GstVideoParseUserData * user_data,
    GstByteReader * br, guint8 field, guint16 provider_code)
{

  guint32 user_data_id = 0;
  guint8 user_data_type_code = 0;
  gboolean a53_process_708_cc_data = FALSE;
  gboolean process_708_em_data = FALSE;
  guint8 temp = 0;
  guint8 cc_count;
  guint cc_size;
  guint bar_size = 0;
  const guint8 *data = NULL;
  guint8 directv_size = 0;


  /* https://en.wikipedia.org/wiki/CEA-708#Picture_User_Data */
  switch (provider_code) {
    case ITU_T_T35_MANUFACTURER_US_ATSC:
      if (!gst_byte_reader_peek_uint32_be (br, &user_data_id)) {
        GST_WARNING_OBJECT (elt, "Missing user data id, ignoring");
        return;
      }
      switch (user_data_id) {
        case A53_USER_DATA_ID_DTG1:
        case A53_USER_DATA_ID_GA94:
          /* ANSI/SCTE 128-2010a section 8.1.2 */
          if (!gst_byte_reader_get_uint32_be (br, &user_data_id)) {
            GST_WARNING_OBJECT (elt, "Missing user data id, ignoring");
            return;
          }
          break;
        default:
          /* check for SCTE 20 */
          if (user_data_id >> 24 == A53_USER_DATA_TYPE_CODE_CC_DATA) {
            user_data_id = USER_DATA_ID_SCTE_20_CC;
            gst_byte_reader_skip (br, 1);
          }
          break;
      }
      break;
    case ITU_T_T35_MANUFACTURER_US_DIRECTV:
      user_data_id = USER_DATA_ID_DIRECTV_CC;
      break;
    default:
      GST_LOG_OBJECT (elt, "Unsupported provider code %d", provider_code);
      return;
  }

  switch (user_data_id) {
    case USER_DATA_ID_SCTE_20_CC:
      GST_DEBUG_OBJECT (elt, "Unsupported SCTE 20 closed captions");
      break;
    case A53_USER_DATA_ID_DTG1:
      if (!gst_byte_reader_get_uint8 (br, &temp)) {
        GST_WARNING_OBJECT (elt, "Missing active format flag, ignoring");
        break;
      }

      /* check active format flag for presence of AFD */
      if (temp & 0x40) {
        if (!gst_byte_reader_get_uint8 (br, &temp)) {
          GST_WARNING_OBJECT (elt,
              "Missing active format description, ignoring");
          break;
        }

        GST_LOG_OBJECT (elt, "parsed active format description (AFD): %d",
            temp);
        user_data->afd_spec = GST_VIDEO_AFD_SPEC_ATSC_A53;
        user_data->afd = temp;
        user_data->active_format_flag = TRUE;
      }
      user_data->field = field;
      break;
    case USER_DATA_ID_DIRECTV_CC:
    case A53_USER_DATA_ID_GA94:
      if (!gst_byte_reader_get_uint8 (br, &user_data_type_code)) {
        GST_WARNING_OBJECT (elt, "Missing user data type code, ignoring");
        break;
      }
      if (provider_code == ITU_T_T35_MANUFACTURER_US_DIRECTV) {
        if (!gst_byte_reader_get_uint8 (br, &directv_size)) {
          GST_WARNING_OBJECT (elt, "Missing DirecTV size, ignoring");
          break;
        }
      }
      switch (user_data_type_code) {
        case A53_USER_DATA_TYPE_CODE_CC_DATA:
          /* 1 (cc count byte) +
           * 1 (reserved byte, 0xff) +
           * 1 (marker_bits, 0xff)
           */
          if (gst_byte_reader_get_remaining (br) < 3) {
            GST_WARNING_OBJECT (elt,
                "Closed caption data packet too short, ignoring");
            break;
          }
          if (!gst_byte_reader_get_uint8 (br, &cc_count)) {
            GST_WARNING_OBJECT (elt, "Missing closed caption count, ignoring");
            break;
          }

          /* A53 part 4 closed captions */
          a53_process_708_cc_data =
              (cc_count & CEA_708_PROCESS_CC_DATA_FLAG) != 0;
          if (!a53_process_708_cc_data) {
            GST_DEBUG_OBJECT (elt,
                "ignoring closed captions as CEA_708_PROCESS_CC_DATA_FLAG is not set");
          }

          process_708_em_data = (cc_count & CEA_708_PROCESS_EM_DATA_FLAG) != 0;
          if (!process_708_em_data) {
            GST_DEBUG_OBJECT (elt,
                "CEA_708_PROCESS_EM_DATA_FLAG flag is not set");
          }
          if (!gst_byte_reader_get_uint8 (br, &temp)) {
            GST_WARNING_OBJECT (elt, "Missing em bits, ignoring");
            break;
          }
          if (temp != 0xff) {
            GST_DEBUG_OBJECT (elt, "em data does not equal 0xFF");
          }
          process_708_em_data = process_708_em_data && (temp == 0xff);
          /* ignore process_708_em_data as there is content that doesn't follow spec for this field */

          if (a53_process_708_cc_data) {
            cc_count = cc_count & 0x1f;
            cc_size = cc_count * 3;

            if (cc_size == 0 || cc_size > gst_byte_reader_get_remaining (br)) {
              GST_DEBUG_OBJECT (elt,
                  "ignoring closed captions, not enough data");
              break;
            }

            /* Shouldn't really happen so let's not go out of our way to handle it */
            if (user_data->closedcaptions_size > 0)
              GST_WARNING_OBJECT (elt, "unused pending closed captions!");

            g_assert (cc_size <= sizeof (user_data->closedcaptions));
            if (!gst_byte_reader_get_data (br, cc_size, &data))
              break;
            memcpy (user_data->closedcaptions, data, cc_size);
            user_data->closedcaptions_size = cc_size;
            user_data->closedcaptions_type = GST_VIDEO_CAPTION_TYPE_CEA708_RAW;
            user_data->field = field;
            GST_DEBUG_OBJECT (elt, "CEA-708 closed captions, %u bytes",
                cc_size);
          }
          break;
        case A53_USER_DATA_TYPE_CODE_SCTE_21_EIA_608_CC_DATA:
          GST_DEBUG_OBJECT (elt, "Unsupported SCTE 21 closed captions");
          break;
        case A53_USER_DATA_TYPE_CODE_BAR_DATA:
          bar_size = gst_byte_reader_get_remaining (br);
          if (bar_size == 0) {
            GST_WARNING_OBJECT (elt, "Bar data packet too short, ignoring");
            break;
          }
          if (bar_size > GST_VIDEO_BAR_MAX_BYTES) {
            GST_WARNING_OBJECT (elt,
                "Bar data packet of size %d is too long, ignoring", bar_size);
            break;
          }
          if (!gst_byte_reader_get_data (br, bar_size, &data))
            break;
          memcpy (user_data->bar_data, data, bar_size);
          user_data->bar_data_size = bar_size;
          user_data->field = field;
          GST_DEBUG_OBJECT (elt, "Bar data, %u bytes", bar_size);
          break;
        default:
          GST_DEBUG_OBJECT (elt,
              "Unrecognized user data type code %d of size %d",
              user_data_type_code, gst_byte_reader_get_remaining (br));
          break;
      }
      break;
    default:
      GST_DEBUG_OBJECT (elt,
          "Unrecognized user data id %d of size %d", user_data_id,
          gst_byte_reader_get_remaining (br));
      break;
  }
}

/*
 * gst_video_push_user_data:
 * @elt: #GstElement that is pushing user data
 * @user_data: #GstVideoParseUserData holding parsed closed caption, bar and AFD data
 * @buf: #GstBuffer that receives the parsed data
 *
 * After user data has been parsed, add the data to @buf
 */
void
gst_video_push_user_data (GstElement * elt, GstVideoParseUserData * user_data,
    GstBuffer * buf)
{
  GstVideoAFD afd;
  GstVideoBarData bar;

  if (user_data->closedcaptions_size == 0) {
    GST_TRACE_OBJECT (elt, "No closed caption data to attach");
  } else if (gst_buffer_get_meta (buf, GST_VIDEO_CAPTION_META_API_TYPE)) {
    GST_DEBUG_OBJECT (elt, "Buffer already has closed caption meta");
  } else {
    gst_buffer_add_video_caption_meta (buf,
        user_data->closedcaptions_type, user_data->closedcaptions,
        user_data->closedcaptions_size);
  }

  if (!user_data->active_format_flag) {
    GST_TRACE_OBJECT (elt, "No AFD to attach");
  } else if (gst_buffer_get_meta (buf, GST_VIDEO_AFD_META_API_TYPE)) {
    GST_DEBUG_OBJECT (elt, "Buffer already has AFD meta");
  } else if (!gst_video_parse_utils_parse_afd (user_data->afd,
          user_data->afd_spec, user_data->field, &afd)) {
    GST_WARNING_OBJECT (elt, "Invalid AFD value %d", user_data->afd);
  } else {
    gst_buffer_add_video_afd_meta (buf, afd.field, afd.spec, afd.afd);
  }

  if (!user_data->bar_data_size) {
    GST_TRACE_OBJECT (elt, "No Bar data to attach");
  } else if (gst_buffer_get_meta (buf, GST_VIDEO_BAR_META_API_TYPE)) {
    GST_DEBUG_OBJECT (elt, "Buffer already has Bar meta");
  } else if (!gst_video_parse_utils_parse_bar (user_data->bar_data,
          user_data->bar_data_size, user_data->field, &bar)) {
    GST_WARNING_OBJECT (elt, "Invalid Bar data");
  } else {
    gst_buffer_add_video_bar_meta (buf, bar.field, bar.is_letterbox,
        bar.bar_data[0], bar.bar_data[1]);
  }
}

/*
 * gst_video_parse_utils_parse_bar:
 * @data: bar data array
 * @size: size of bar data array
 * @field: #GstVideoParseUtilsField we're parsing for
 * @bar: #GstVideoBarData structure
 *
 * Parse bar data bytes into #GstVideoBarData structure
 *
 * See Table in https://www.atsc.org/wp-content/uploads/2015/03/a_53-Part-4-2009.pdf
 *
 * Returns: TRUE if parsing was successful, otherwise FALSE
 */
static gboolean
gst_video_parse_utils_parse_bar (const guint8 * data, gsize size,
    GstVideoParseUtilsField field, GstVideoBarData * bar)
{
  guint8 temp;
  int i = 0;
  guint8 bar_flags[4];
  guint16 bar_vals[4] = { 0, 0, 0, 0 };
  GstBitReader bar_tender;

  g_return_val_if_fail (bar != NULL, FALSE);

  if (size == 0 || size > GST_VIDEO_BAR_MAX_BYTES)
    return FALSE;

  gst_bit_reader_init (&bar_tender, data, size);

  /* parse bar flags */
  for (i = 0; i < 4; ++i) {
    if (!gst_bit_reader_get_bits_uint8 (&bar_tender, bar_flags + i, 1))
      return FALSE;
  }

  /* the next four bits must equal 1111 */
  if (!gst_bit_reader_get_bits_uint8 (&bar_tender, &temp, 4) || temp != 0xF)
    return FALSE;

  /* parse bar values */
  for (i = 0; i < 4; ++i) {
    if (bar_flags[i]) {
      /* the next two bits must equal 11 */
      if (!gst_bit_reader_get_bits_uint8 (&bar_tender, &temp, 2) || temp != 0x3)
        return FALSE;

      if (!gst_bit_reader_get_bits_uint16 (&bar_tender, bar_vals + i, 14))
        return FALSE;
    }
  }

  /* bars are signaled in pairs: either top/bottom or left/right, but not both */
  if ((bar_flags[0] != bar_flags[1]) || (bar_flags[2] != bar_flags[3]))
    return FALSE;
  if (bar_flags[0] && bar_flags[2])
    return FALSE;

  bar->field = field;
  bar->is_letterbox = bar_flags[0];
  if (bar->is_letterbox) {
    bar->bar_data[0] = bar_vals[0];
    bar->bar_data[1] = bar_vals[1];
  } else {
    bar->bar_data[0] = bar_vals[2];
    bar->bar_data[1] = bar_vals[3];
  }
  return TRUE;
}

/*
 * gst_video_parse_utils_parse_afd:
 * @data: bar byte
 * @spec: #GstVideoAFDSpec indicating specification that applies to AFD byte
 * @field: #GstVideoParseUtilsField
 * @afd: pointer to #GstVideoAFD struct
 *
 * Parse afd byte into #GstVideoAFD struct
 *
 * See:
 *
 * https://www.atsc.org/wp-content/uploads/2015/03/a_53-Part-4-2009.pdf
 *
 * https://en.wikipedia.org/wiki/Active_Format_Description#Complete_list_of_AFD_codes
 *
 * and
 *
 * SMPTE ST2016-1
 *
 * Returns: TRUE if parsing was successful, otherwise FALSE
 */
static gboolean
gst_video_parse_utils_parse_afd (const guint8 data, GstVideoAFDSpec spec,
    GstVideoParseUtilsField field, GstVideoAFD * afd)
{
  GstVideoAFDAspectRatio aspect_ratio = GST_VIDEO_AFD_ASPECT_RATIO_UNDEFINED;
  GstVideoAFDValue afd_data;

  g_return_val_if_fail (afd != NULL, FALSE);

  switch (spec) {
    case GST_VIDEO_AFD_SPEC_DVB_ETSI:
    case GST_VIDEO_AFD_SPEC_ATSC_A53:
      if ((data & 0x40) == 0)
        return FALSE;

      afd_data = data & 0xF;
      break;
    case GST_VIDEO_AFD_SPEC_SMPTE_ST2016_1:
      if ((data & 0x80) || (data & 0x3))
        return FALSE;

      afd_data = (data >> 3) & 0xF;
      aspect_ratio = (GstVideoAFDAspectRatio) (((data >> 2) & 1) + 1);
      break;
    default:
      g_return_val_if_reached (FALSE);
  }

  switch (afd_data) {
    case GST_VIDEO_AFD_UNAVAILABLE:
      if (spec == GST_VIDEO_AFD_SPEC_DVB_ETSI) {
        /* reserved for DVB/ETSI */
        return FALSE;
      }
      break;

    case GST_VIDEO_AFD_16_9_TOP_ALIGNED:
    case GST_VIDEO_AFD_14_9_TOP_ALIGNED:
    case GST_VIDEO_AFD_GREATER_THAN_16_9:
    case GST_VIDEO_AFD_4_3_FULL_16_9_FULL:
    case GST_VIDEO_AFD_4_3_FULL_4_3_PILLAR:
    case GST_VIDEO_AFD_16_9_LETTER_16_9_FULL:
    case GST_VIDEO_AFD_14_9_LETTER_14_9_PILLAR:
    case GST_VIDEO_AFD_4_3_FULL_14_9_CENTER:
    case GST_VIDEO_AFD_16_9_LETTER_14_9_CENTER:
    case GST_VIDEO_AFD_16_9_LETTER_4_3_CENTER:
      break;

    default:
      return FALSE;
  }

  afd->field = field;
  afd->aspect_ratio = aspect_ratio;
  afd->spec = spec;
  afd->afd = afd_data;
  return TRUE;
}

/*
 * gst_video_clear_user_data:
 * @user_data: #GstVideoParseUserData struct to hold parsed closed caption, bar and AFD data
 *
 * Clears the user data, resetting it for the next frame
 */
void
gst_video_clear_user_data (GstVideoParseUserData * user_data)
{
  user_data->closedcaptions_size = 0;
  user_data->bar_data_size = 0;
  user_data->active_format_flag = 0;
}

/*
 * gst_video_parse_user_data_unregistered:
 * @elt: #GstElement that is parsing user data
 * @user_data: #GstVideoParseUserDataUnregistered to hold SEI User Data Unregistered
 * @br: #GstByteReader attached to buffer of user data
 * @uuid: User Data Unregistered UUID
 *
 * Copy remaining bytes in @br and store in @user_data
 */
void
gst_video_parse_user_data_unregistered (GstElement * elt,
    GstVideoParseUserDataUnregistered * user_data,
    GstByteReader * br, guint8 uuid[16])
{
  GstVideoUnregisteredMessage msg;

  memcpy (&msg.uuid, uuid, 16);

  msg.size = gst_byte_reader_get_size (br);
  if (!gst_byte_reader_dup_data (br, msg.size, &msg.data)) {
    g_return_if_reached ();
  }

  if (user_data->messages == NULL) {
    user_data->messages = g_array_sized_new (FALSE, TRUE,
        sizeof (GstVideoUnregisteredMessage), 3);
    g_array_set_clear_func (user_data->messages,
        (GDestroyNotify) gst_video_clear_unregistered_message);
  }

  g_array_append_val (user_data->messages, msg);
}

/*
 * gst_video_push_user_data_unregistered:
 * @elt: #GstElement that is pushing user data
 * @user_data: #GstVideoParseUserDataUnregistered holding SEI User Data Unregistered
 * @buf: (transfer none): #GstBuffer that receives the unregistered data
 *
 * Attach unregistered messages from @user_data to @buf as
 * GstVideoSEIUserDataUnregisteredMeta
 */
void
gst_video_push_user_data_unregistered (GstElement * elt,
    GstVideoParseUserDataUnregistered * user_data, GstBuffer * buf)
{
  GArray *messages = user_data->messages;
  guint i;

  if (messages == NULL || messages->len == 0) {
    GST_TRACE_OBJECT (elt, "No unregistered user data to attach");
    return;
  }

  if (gst_buffer_get_meta (buf,
          GST_VIDEO_SEI_USER_DATA_UNREGISTERED_META_API_TYPE)) {
    GST_DEBUG_OBJECT (elt, "Buffer already has unregistered meta");
    return;
  }

  for (i = 0; i < messages->len; i++) {
    GstVideoUnregisteredMessage *msg =
        &g_array_index (messages, GstVideoUnregisteredMessage, i);

    gst_buffer_add_video_sei_user_data_unregistered_meta (buf, msg->uuid,
        msg->data, msg->size);
  }
}

/*
 * gst_video_clear_user_data_unregistered:
 * @user_data: #GstVideoParseUserDataUnregistered holding SEI User Data Unregistered
 * @free: Whether to deallocate memory resources
 *
 * Clears the user data unregistered, resetting it for the next frame
 */
void gst_video_clear_user_data_unregistered
    (GstVideoParseUserDataUnregistered * user_data, gboolean free)
{
  if (free)
    g_clear_pointer (&user_data->messages, g_array_unref);
  else if (user_data->messages != NULL)
    g_array_set_size (user_data->messages, 0);
}

/*
 * gst_video_clear_unregistered_message:
 * @msg: #GstVideoUnregisteredMessage holding a single SEI User Data Unregistered
 *
 * Clears the message, freeing the data
 */
static void
gst_video_clear_unregistered_message (GstVideoUnregisteredMessage * msg)
{
  memset (&msg->uuid, 0, sizeof msg->uuid);
  g_clear_pointer (&msg->data, g_free);
  msg->size = 0;
}
