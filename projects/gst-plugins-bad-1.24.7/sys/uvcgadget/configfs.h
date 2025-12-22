/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * ConfigFS Gadget device handling
 *
 * Copyright (C) 2018 Kieran Bingham
 *
 * Contact: Kieran Bingham <kieran.bingham@ideasonboard.com>
 */

#ifndef __CONFIGFS_H__
#define __CONFIGFS_H__

#include <stdint.h>

/*
 * struct uvc_function_config_endpoint - Endpoint parameters
 * @bInterval: Transfer interval (interrupt and isochronous only)
 * @bMaxBurst: Transfer burst size (super-speed only)
 * @wMaxPacketSize: Maximum packet size (including the multiplier)
 */
struct uvc_function_config_endpoint
{
  unsigned int bInterval;
  unsigned int bMaxBurst;
  unsigned int wMaxPacketSize;
};

/*
 * struct uvc_function_config_interface - Interface parameters
 * @bInterfaceNumber: Interface number
 */
struct uvc_function_config_interface
{
  unsigned int bInterfaceNumber;
};

/*
 * struct uvc_function_config_control - Control interface parameters
 * @intf: Generic interface parameters
 */
struct uvc_function_config_control
{
  struct uvc_function_config_interface intf;
};

/*
 * struct uvc_function_config_frame - Streaming frame parameters
 * @index: Frame index in the UVC descriptors
 * @width: Frame width in pixels
 * @height: Frame height in pixels
 * @num_intervals: Number of entries in the intervals array
 * @intervals: Array of frame intervals
 */
struct uvc_function_config_frame
{
  unsigned int index;
  unsigned int width;
  unsigned int height;
  unsigned int num_intervals;
  unsigned int *intervals;
  unsigned int maxvideofbsize;
};

/*
 * struct uvc_function_config_format - Streaming format parameters
 * @index: Format index in the UVC descriptors
 * @guid: Format GUID
 * @fcc: V4L2 pixel format
 * @num_frames: Number of entries in the frames array
 * @frames: Array of frame descriptors
 */
struct uvc_function_config_format
{
  unsigned int index;
  uint8_t guid[16];
  unsigned int fcc;
  unsigned int num_frames;
  struct uvc_function_config_frame *frames;
};

/*
 * struct uvc_function_config_streaming - Streaming interface parameters
 * @intf: Generic interface parameters
 * @ep: Endpoint parameters
 * @num_formats: Number of entries in the formats array
 * @formats: Array of format descriptors
 */
struct uvc_function_config_streaming
{
  struct uvc_function_config_interface intf;
  struct uvc_function_config_endpoint ep;

  unsigned int num_formats;
  struct uvc_function_config_format *formats;
};

/*
 * struct uvc_function_config - UVC function configuration parameters
 * @video: Full path to the video device node
 * @udc: UDC name
 * @control: Control interface configuration
 * @streaming: Streaming interface configuration
 */
struct uvc_function_config
{
  char *video;
  char *udc;

  struct uvc_function_config_control control;
  struct uvc_function_config_streaming streaming;
};

struct uvc_function_config *configfs_parse_uvc_function (const char *function);
void configfs_free_uvc_function (struct uvc_function_config *fc);

struct uvc_function_config *configfs_parse_uvc_videodev (int fd,
    const char *video);

#endif
