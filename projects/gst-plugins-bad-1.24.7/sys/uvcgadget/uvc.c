/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Copyright (C) 2023 Pengutronix e.K. - www.pengutronix.de
 *
 */

#define _GNU_SOURCE

#include <endian.h>
#include <glob.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include "gstuvcsink.h"
#include "configfs.h"

#define GST_CAT_DEFAULT uvcsink_debug

#define UVC_STRING_CONTROL_IDX                  0
#define UVC_STRING_STREAMING_IDX                1

/* --------------------------------------------------------------------------
 * Control requests
 */

static const char *
uvc_request_name (uint8_t req)
{
  switch (req) {
    case UVC_SET_CUR:
      return "SET_CUR";
    case UVC_GET_CUR:
      return "GET_CUR";
    case UVC_GET_MIN:
      return "GET_MIN";
    case UVC_GET_MAX:
      return "GET_MAX";
    case UVC_GET_RES:
      return "GET_RES";
    case UVC_GET_LEN:
      return "GET_LEN";
    case UVC_GET_INFO:
      return "GET_INFO";
    case UVC_GET_DEF:
      return "GET_DEF";
    default:
      return "<invalid>";
  }
}

static const char *
uvc_video_control_interface_control_selector_name (uint8_t cs)
{
  switch (cs) {
    case UVC_VC_CONTROL_UNDEFINED:
      return "UVC_VC_CONTROL_UNDEFINED";
    case UVC_VC_VIDEO_POWER_MODE_CONTROL:
      return "UVC_VC_VIDEO_POWER_MODE_CONTROL";
    case UVC_VC_REQUEST_ERROR_CODE_CONTROL:
      return "UVC_VC_REQUEST_ERROR_CODE_CONTROL";
    default:
      return "<unknown video control interface control selector>";
  }
}

static const char *
uvc_camera_terminal_control_selector_name (uint8_t cs)
{
  switch (cs) {
    case UVC_CT_CONTROL_UNDEFINED:
      return "UVC_CT_CONTROL_UNDEFINED";
    case UVC_CT_SCANNING_MODE_CONTROL:
      return "UVC_CT_SCANNING_MODE_CONTROL";
    case UVC_CT_AE_MODE_CONTROL:
      return "UVC_CT_AE_MODE_CONTROL";
    case UVC_CT_AE_PRIORITY_CONTROL:
      return "UVC_CT_AE_PRIORITY_CONTROL";
    case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
      return "UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL";
    case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
      return "UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL";
    case UVC_CT_FOCUS_ABSOLUTE_CONTROL:
      return "UVC_CT_FOCUS_ABSOLUTE_CONTROL";
    case UVC_CT_FOCUS_RELATIVE_CONTROL:
      return "UVC_CT_FOCUS_RELATIVE_CONTROL";
    case UVC_CT_FOCUS_AUTO_CONTROL:
      return "UVC_CT_FOCUS_AUTO_CONTROL";
    case UVC_CT_IRIS_ABSOLUTE_CONTROL:
      return "UVC_CT_IRIS_ABSOLUTE_CONTROL";
    case UVC_CT_IRIS_RELATIVE_CONTROL:
      return "UVC_CT_IRIS_RELATIVE_CONTROL";
    case UVC_CT_ZOOM_ABSOLUTE_CONTROL:
      return "UVC_CT_ZOOM_ABSOLUTE_CONTROL";
    case UVC_CT_ZOOM_RELATIVE_CONTROL:
      return "UVC_CT_ZOOM_RELATIVE_CONTROL";
    case UVC_CT_PANTILT_ABSOLUTE_CONTROL:
      return "UVC_CT_PANTILT_ABSOLUTE_CONTROL";
    case UVC_CT_PANTILT_RELATIVE_CONTROL:
      return "UVC_CT_PANTILT_RELATIVE_CONTROL";
    case UVC_CT_ROLL_ABSOLUTE_CONTROL:
      return "UVC_CT_ROLL_ABSOLUTE_CONTROL";
    case UVC_CT_ROLL_RELATIVE_CONTROL:
      return "UVC_CT_ROLL_RELATIVE_CONTROL";
    case UVC_CT_PRIVACY_CONTROL:
      return "UVC_CT_PRIVACY_CONTROL";
    default:
      return "<unknown camera terminal control selector>";
  }
}

static const char *
uvc_processing_unit_control_selector_name (uint8_t cs)
{
  switch (cs) {
    case UVC_PU_CONTROL_UNDEFINED:
      return "UVC_PU_CONTROL_UNDEFINED";
    case UVC_PU_BACKLIGHT_COMPENSATION_CONTROL:
      return "UVC_PU_BACKLIGHT_COMPENSATION_CONTROL";
    case UVC_PU_BRIGHTNESS_CONTROL:
      return "UVC_PU_BRIGHTNESS_CONTROL";
    case UVC_PU_CONTRAST_CONTROL:
      return "UVC_PU_CONTRAST_CONTROL";
    case UVC_PU_GAIN_CONTROL:
      return "UVC_PU_GAIN_CONTROL";
    case UVC_PU_POWER_LINE_FREQUENCY_CONTROL:
      return "UVC_PU_POWER_LINE_FREQUENCY_CONTROL";
    case UVC_PU_HUE_CONTROL:
      return "UVC_PU_HUE_CONTROL";
    case UVC_PU_SATURATION_CONTROL:
      return "UVC_PU_SATURATION_CONTROL";
    case UVC_PU_SHARPNESS_CONTROL:
      return "UVC_PU_SHARPNESS_CONTROL";
    case UVC_PU_GAMMA_CONTROL:
      return "UVC_PU_GAMMA_CONTROL";
    case UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL:
      return "UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL";
    case UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:
      return "UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL";
    case UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL:
      return "UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL";
    case UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL:
      return "UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL";
    case UVC_PU_DIGITAL_MULTIPLIER_CONTROL:
      return "UVC_PU_DIGITAL_MULTIPLIER_CONTROL";
    case UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL:
      return "UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL";
    case UVC_PU_HUE_AUTO_CONTROL:
      return "UVC_PU_HUE_AUTO_CONTROL";
    case UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL:
      return "UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL";
    case UVC_PU_ANALOG_LOCK_STATUS_CONTROL:
      return "UVC_PU_ANALOG_LOCK_STATUS_CONTROL";
    default:
      return "<unknown processing unit control selector>";
  }
}

static const char *
uvc_video_streaming_interface_control_selector_name (uint8_t cs)
{
  switch (cs) {
    case UVC_VS_CONTROL_UNDEFINED:
      return "UVC_VS_CONTROL_UNDEFINED";
    case UVC_VS_PROBE_CONTROL:
      return "UVC_VS_PROBE_CONTROL";
    case UVC_VS_COMMIT_CONTROL:
      return "UVC_VS_COMMIT_CONTROL";
    case UVC_VS_STILL_PROBE_CONTROL:
      return "UVC_VS_STILL_PROBE_CONTROL";
    case UVC_VS_STILL_COMMIT_CONTROL:
      return "UVC_VS_STILL_COMMIT_CONTROL";
    case UVC_VS_STILL_IMAGE_TRIGGER_CONTROL:
      return "UVC_VS_STILL_IMAGE_TRIGGER_CONTROL";
    case UVC_VS_STREAM_ERROR_CODE_CONTROL:
      return "UVC_VS_STREAM_ERROR_CODE_CONTROL";
    case UVC_VS_GENERATE_KEY_FRAME_CONTROL:
      return "UVC_VS_GENERATE_KEY_FRAME_CONTROL";
    case UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL:
      return "UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL";
    case UVC_VS_SYNC_DELAY_CONTROL:
      return "UVC_VS_SYNC_DELAY_CONTROL";
    default:
      return "<unknown video streaming interface control selector>";
  }
}

int
uvc_fill_streaming_control (GstUvcSink * self,
    struct uvc_streaming_control *ctrl,
    int iframe, int iformat, unsigned int dwival)
{
  const struct uvc_function_config_format *format;
  const struct uvc_function_config_frame *frame;
  unsigned int i;

  if (!self->fc)
    return -ENOENT;
  /*
   * Restrict the iformat, iframe and ival to valid values. Negative
   * values for iformat or iframe will result in the maximum valid value
   * being selected.
   */
  iformat = CLAMP ((unsigned int) iformat, 1U, self->fc->streaming.num_formats);
  format = &self->fc->streaming.formats[iformat - 1];

  iframe = CLAMP ((unsigned int) iframe, 1U, format->num_frames);
  frame = &format->frames[iframe - 1];

  for (i = 0; i < frame->num_intervals; ++i) {
    if (dwival <= frame->intervals[i]) {
      dwival = frame->intervals[i];
      break;
    }
  }

  if (i == frame->num_intervals)
    dwival = frame->intervals[frame->num_intervals - 1];

  memset (ctrl, 0, sizeof (*ctrl));

  ctrl->bmHint = 1;
  ctrl->bFormatIndex = iformat;
  ctrl->bFrameIndex = iframe;
  ctrl->dwFrameInterval = dwival;

  switch (format->fcc) {
    case V4L2_PIX_FMT_YUYV:
      ctrl->dwMaxVideoFrameSize = frame->width * frame->height * 2;
      break;
    case V4L2_PIX_FMT_MJPEG:
      ctrl->dwMaxVideoFrameSize = frame->maxvideofbsize;
      break;
  }

  ctrl->dwMaxPayloadTransferSize = self->fc->streaming.ep.wMaxPacketSize;
  ctrl->bmFramingInfo = 3;
  ctrl->bPreferedVersion = 1;
  ctrl->bMaxVersion = 1;

  return 0;
}

int
uvc_events_process_data (GstUvcSink * self, const struct uvc_request_data *data)
{
  const struct uvc_streaming_control *ctrl =
      (const struct uvc_streaming_control *) &data->data;
  struct uvc_streaming_control *target;
  int ret;

  switch (self->control) {
    case UVC_VS_PROBE_CONTROL:
      GST_DEBUG_OBJECT (self, "setting probe control");
      target = &self->probe;
      break;

    case UVC_VS_COMMIT_CONTROL:
      GST_DEBUG_OBJECT (self, "setting commit control");
      target = &self->commit;
      break;

    default:
      GST_ELEMENT_ERROR (self, RESOURCE, READ,
          ("setting unknown control, %d", self->control), NULL);
      return -EOPNOTSUPP;
  }

  ret = uvc_fill_streaming_control (self, target, ctrl->bFrameIndex,
      ctrl->bFormatIndex, ctrl->dwFrameInterval);
  if (ret)
    return ret;

  if (self->control == UVC_VS_COMMIT_CONTROL) {
    self->cur.bFrameIndex = ctrl->bFrameIndex;
    self->cur.bFormatIndex = ctrl->bFormatIndex;
    self->cur.dwFrameInterval = ctrl->dwFrameInterval;
  }

  return 0;
}

static int
uvc_events_process_streaming (GstUvcSink * self, uint8_t req, uint8_t cs,
    struct uvc_request_data *resp)
{
  struct uvc_streaming_control *ctrl;
  int ret;

  GST_DEBUG_OBJECT (self,
      "%s: %s",
      uvc_video_streaming_interface_control_selector_name (cs),
      uvc_request_name (req));

  if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
    return 0;

  ctrl = (struct uvc_streaming_control *) &resp->data;
  resp->length = sizeof (*ctrl);

  switch (req) {
    case UVC_SET_CUR:
      self->control = cs;
      resp->length = 34;
      break;

    case UVC_GET_CUR:
      if (cs == UVC_VS_PROBE_CONTROL)
        memcpy (ctrl, &self->probe, sizeof (*ctrl));
      else
        memcpy (ctrl, &self->commit, sizeof (*ctrl));
      break;

    case UVC_GET_MIN:
    case UVC_GET_MAX:
    case UVC_GET_DEF:
      if (req == UVC_GET_MAX)
        ret = uvc_fill_streaming_control (self, ctrl, -1, -1, UINT_MAX);
      else
        ret = uvc_fill_streaming_control (self, ctrl, 1, 1, 0);
      if (ret)
        return ret;
      break;

    case UVC_GET_RES:
      memset (ctrl, 0, sizeof (*ctrl));
      break;

    case UVC_GET_LEN:
      resp->data[0] = 0x00;
      resp->data[1] = 0x22;
      resp->length = 2;
      break;

    case UVC_GET_INFO:
      resp->data[0] = 0x03;
      resp->length = 1;
      break;
  }

  return 0;
}

static void
uvc_events_parse_control (GstUvcSink * self, uint8_t req,
    uint8_t cs, uint8_t entity_id, uint8_t len, struct uvc_request_data *resp)
{
  switch (entity_id) {
    case 0:
      GST_DEBUG_OBJECT (self, "%s",
          uvc_video_control_interface_control_selector_name (cs));
      break;

    case 1:
      GST_DEBUG_OBJECT (self, "%s: %s",
          uvc_camera_terminal_control_selector_name (cs),
          uvc_request_name (req));
      break;

    case 2:
      GST_DEBUG_OBJECT (self, "%s: %s",
          uvc_processing_unit_control_selector_name (cs),
          uvc_request_name (req));
      break;

    default:
      GST_DEBUG_OBJECT (self,
          "Unknown entity ID (0x%02x), CS: 0x%02x, Request %s (0x%02x)",
          entity_id, cs, uvc_request_name (req), req);
      break;
  }
}

static int
uvc_events_process_class (GstUvcSink * self,
    const struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
  unsigned int interface = le16toh (ctrl->wIndex) & 0xff;

  if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
    return -EINVAL;

  if (interface == UVC_STRING_CONTROL_IDX) {
    uvc_events_parse_control (self, ctrl->bRequest, ctrl->wValue >> 8,
        ctrl->wIndex >> 8, ctrl->wLength, resp);
    return -EOPNOTSUPP;
  } else if (interface == UVC_STRING_STREAMING_IDX) {
    return uvc_events_process_streaming (self, ctrl->bRequest,
        le16toh (ctrl->wValue) >> 8, resp);
  }

  return 0;
}

int
uvc_events_process_setup (GstUvcSink * self,
    const struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
  self->control = 0;

  GST_DEBUG_OBJECT (self,
      "bRequestType %02x bRequest %02x wValue %04x wIndex %04x wLength %04x",
      ctrl->bRequestType, ctrl->bRequest, ctrl->wValue,
      ctrl->wIndex, ctrl->wLength);

  switch (ctrl->bRequestType & USB_TYPE_MASK) {
    case USB_TYPE_STANDARD:
      return -EOPNOTSUPP;

    case USB_TYPE_CLASS:
      return uvc_events_process_class (self, ctrl, resp);

    default:
      break;
  }

  return 0;
}
