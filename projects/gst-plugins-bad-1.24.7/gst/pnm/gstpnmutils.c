/* GStreamer PNM utility functions
 * Copyright (C) 2009 Lutz Mueller <lutz@users.sourceforge.net>
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

#include "gstpnmutils.h"

GstPnmInfoMngrResult
gst_pnm_info_mngr_scan (GstPnmInfoMngr * mngr, const guint8 * buf,
    guint buf_len)
{
  guint i = 0;

  g_return_val_if_fail (mngr != NULL, GST_PNM_INFO_MNGR_RESULT_FAILED);
  g_return_val_if_fail (buf || !buf_len, GST_PNM_INFO_MNGR_RESULT_FAILED);

  if (!buf_len)
    return (mngr->info.fields ==
        GST_PNM_INFO_FIELDS_ALL) ? GST_PNM_INFO_MNGR_RESULT_FINISHED :
        GST_PNM_INFO_MNGR_RESULT_READING;

  switch (mngr->state) {
    case GST_PNM_INFO_MNGR_STATE_COMMENT:
      for (i = 0; (i < buf_len) && (buf[i] != '\n'); i++);
      if (i == buf_len)
        return GST_PNM_INFO_MNGR_RESULT_READING;
      mngr->state = GST_PNM_INFO_MNGR_STATE_NONE;
      mngr->data_offset += i;
      return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
    case GST_PNM_INFO_MNGR_STATE_WHITE_SPACE:
      for (i = 0; (i < buf_len) && ((buf[i] == ' ') || (buf[i] == '\t')
              || (buf[i] == '\n')); i++);
      if (i == buf_len)
        return GST_PNM_INFO_MNGR_RESULT_READING;
      mngr->state = GST_PNM_INFO_MNGR_STATE_NONE;
      mngr->data_offset += i;
      return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
    case GST_PNM_INFO_MNGR_STATE_NONE:
      switch (buf[i++]) {
        case '#':
          mngr->state = GST_PNM_INFO_MNGR_STATE_COMMENT;
          mngr->data_offset += i;
          return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
        case ' ':
        case '\t':
        case '\n':
          mngr->state = GST_PNM_INFO_MNGR_STATE_WHITE_SPACE;
          mngr->data_offset += i;
          return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
        case 'P':
          if (mngr->info.fields & GST_PNM_INFO_FIELDS_TYPE)
            return GST_PNM_INFO_MNGR_RESULT_FAILED;
          mngr->state = GST_PNM_INFO_MNGR_STATE_DATA_TYPE;
          mngr->data_offset += i;
          return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (mngr->info.fields & GST_PNM_INFO_FIELDS_MAX)
            return GST_PNM_INFO_MNGR_RESULT_FINISHED;
          if (mngr->info.fields & GST_PNM_INFO_FIELDS_HEIGHT) {
            mngr->state = GST_PNM_INFO_MNGR_STATE_DATA_MAX;
            return gst_pnm_info_mngr_scan (mngr, buf, buf_len);
          }
          if (mngr->info.fields & GST_PNM_INFO_FIELDS_WIDTH) {
            mngr->state = GST_PNM_INFO_MNGR_STATE_DATA_HEIGHT;
            return gst_pnm_info_mngr_scan (mngr, buf, buf_len);
          }
          mngr->state = GST_PNM_INFO_MNGR_STATE_DATA_WIDTH;
          return gst_pnm_info_mngr_scan (mngr, buf, buf_len);
        default:
          return GST_PNM_INFO_MNGR_RESULT_FAILED;
      }
    case GST_PNM_INFO_MNGR_STATE_DATA_TYPE:
      switch (buf[i++]) {
        case '1':
          mngr->info.type = GST_PNM_TYPE_BITMAP;
          mngr->info.encoding = GST_PNM_ENCODING_ASCII;
          break;
        case '2':
          mngr->info.type = GST_PNM_TYPE_GRAYMAP;
          mngr->info.encoding = GST_PNM_ENCODING_ASCII;
          break;
        case '3':
          mngr->info.type = GST_PNM_TYPE_PIXMAP;
          mngr->info.encoding = GST_PNM_ENCODING_ASCII;
          break;
        case '4':
          mngr->info.type = GST_PNM_TYPE_BITMAP;
          mngr->info.encoding = GST_PNM_ENCODING_RAW;
          break;
        case '5':
          mngr->info.type = GST_PNM_TYPE_GRAYMAP;
          mngr->info.encoding = GST_PNM_ENCODING_RAW;
          break;
        case '6':
          mngr->info.type = GST_PNM_TYPE_PIXMAP;
          mngr->info.encoding = GST_PNM_ENCODING_RAW;
          break;
        default:
          return GST_PNM_INFO_MNGR_RESULT_FAILED;
      }
      mngr->info.fields |=
          GST_PNM_INFO_FIELDS_TYPE | GST_PNM_INFO_FIELDS_ENCODING;
      mngr->state = GST_PNM_INFO_MNGR_STATE_WHITE_SPACE;
      if (i == buf_len)
        return GST_PNM_INFO_MNGR_RESULT_READING;
      mngr->info.width = mngr->info.height = mngr->info.max = 0;
      mngr->data_offset += i;
      return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
    case GST_PNM_INFO_MNGR_STATE_DATA_WIDTH:
      if ((buf[i] < '0') || (buf[i] > '9')) {
        switch (buf[i]) {
          case '\n':
          case '\t':
          case ' ':
            mngr->info.fields |= GST_PNM_INFO_FIELDS_WIDTH;
            mngr->state = GST_PNM_INFO_MNGR_STATE_WHITE_SPACE;
            mngr->data_offset += i;
            return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
          default:
            return GST_PNM_INFO_MNGR_RESULT_FAILED;
        }
      }
      mngr->info.width *= 10;
      mngr->info.width += buf[i++] - 0x030;
      mngr->data_offset += i;
      return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
    case GST_PNM_INFO_MNGR_STATE_DATA_HEIGHT:
      if ((buf[i] < '0') || (buf[i] > '9')) {
        switch (buf[i]) {
          case '\n':
          case '\t':
          case ' ':
            mngr->info.fields |= GST_PNM_INFO_FIELDS_HEIGHT;
            mngr->state = GST_PNM_INFO_MNGR_STATE_WHITE_SPACE;
            mngr->data_offset += i;
            if (mngr->info.type == GST_PNM_TYPE_BITMAP) {
              mngr->data_offset += 1;
              mngr->info.fields |= GST_PNM_INFO_FIELDS_MAX;
              return GST_PNM_INFO_MNGR_RESULT_FINISHED;
            }
            return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
          default:
            return GST_PNM_INFO_MNGR_RESULT_FAILED;
        }
      }
      mngr->info.height *= 10;
      mngr->info.height += buf[i++] - 0x030;
      mngr->data_offset += i;
      return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
    case GST_PNM_INFO_MNGR_STATE_DATA_MAX:
      if ((buf[i] < '0') || (buf[i] > '9')) {
        switch (buf[i]) {
          case '\n':
          case '\t':
          case ' ':
            /* Check for maximum and minimum supported bit depth and
               return error if its out of range */
            if (mngr->info.type == GST_PNM_TYPE_GRAYMAP) {
              if ((mngr->info.max > 65535) || (mngr->info.max < 1)) {
                return GST_PNM_INFO_MNGR_RESULT_FAILED;
              }
            } else if ((mngr->info.max > 255) || (mngr->info.max < 1)) {
              return GST_PNM_INFO_MNGR_RESULT_FAILED;
            }
            mngr->info.fields |= GST_PNM_INFO_FIELDS_MAX;
            mngr->data_offset += i + 1;
            return GST_PNM_INFO_MNGR_RESULT_FINISHED;
          default:
            return GST_PNM_INFO_MNGR_RESULT_FAILED;
        }
      }
      mngr->info.max *= 10;
      mngr->info.max += buf[i++] - 0x030;
      mngr->data_offset += i;
      return gst_pnm_info_mngr_scan (mngr, buf + i, buf_len - i);
  }
  return GST_PNM_INFO_MNGR_RESULT_FAILED;
}
