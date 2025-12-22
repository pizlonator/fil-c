/* iSAC plugin utils
 *
 * Copyright (C) 2020 Collabora Ltd.
 *  Author: Guillaume Desmottes <guillaume.desmottes@collabora.com>, Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#include "gstisacutils.h"

#include <modules/audio_coding/codecs/isac/main/source/settings.h>

const gchar *
isac_error_code_to_str (gint code)
{
  switch (code) {
    case ISAC_MEMORY_ALLOCATION_FAILED:
      return "allocation failed";
    case ISAC_MODE_MISMATCH:
      return "mode mismatch";
    case ISAC_DISALLOWED_BOTTLENECK:
      return "disallowed bottleneck";
    case ISAC_DISALLOWED_FRAME_LENGTH:
      return "disallowed frame length";
    case ISAC_UNSUPPORTED_SAMPLING_FREQUENCY:
      return "unsupported sampling frequency";
    case ISAC_RANGE_ERROR_BW_ESTIMATOR:
      return "range error bandwitch estimator";
    case ISAC_ENCODER_NOT_INITIATED:
      return "encoder not initiated";
    case ISAC_DISALLOWED_CODING_MODE:
      return "disallowed coding mode";
    case ISAC_DISALLOWED_FRAME_MODE_ENCODER:
      return "disallowed frame mode encoder";
    case ISAC_DISALLOWED_BITSTREAM_LENGTH:
      return "disallowed bitstream length";
    case ISAC_PAYLOAD_LARGER_THAN_LIMIT:
      return "payload larger than limit";
    case ISAC_DISALLOWED_ENCODER_BANDWIDTH:
      return "disallowed encoder bandwith";
    case ISAC_DECODER_NOT_INITIATED:
      return "decoder not initiated";
    case ISAC_EMPTY_PACKET:
      return "empty packet";
    case ISAC_DISALLOWED_FRAME_MODE_DECODER:
      return "disallowed frame mode decoder";
    case ISAC_RANGE_ERROR_DECODE_FRAME_LENGTH:
      return "range error decode frame length";
    case ISAC_RANGE_ERROR_DECODE_BANDWIDTH:
      return "range error decode bandwith";
    case ISAC_RANGE_ERROR_DECODE_PITCH_GAIN:
      return "range error decode pitch gain";
    case ISAC_RANGE_ERROR_DECODE_PITCH_LAG:
      return "range error decode pitch lag";
    case ISAC_RANGE_ERROR_DECODE_LPC:
      return "range error decode lpc";
    case ISAC_RANGE_ERROR_DECODE_SPECTRUM:
      return "range error decode spectrum";
    case ISAC_LENGTH_MISMATCH:
      return "length mismatch";
    case ISAC_RANGE_ERROR_DECODE_BANDWITH:
      return "range error decode bandwith";
    case ISAC_DISALLOWED_BANDWIDTH_MODE_DECODER:
      return "disallowed bandwitch mode decoder";
    case ISAC_DISALLOWED_LPC_MODEL:
      return "disallowed lpc model";
    case ISAC_INCOMPATIBLE_FORMATS:
      return "incompatible formats";
  }

  return "<unknown>";
}
