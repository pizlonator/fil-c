/*
 * gstrtponviftimestamp-parse.h
 *
 * Copyright (C) 2014 Axis Communications AB
 *  Author: Guillaume Desmottes <guillaume.desmottes@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GST_RTP_ONVIF_PARSE_H__
#define __GST_RTP_ONVIF_PARSE_H__


#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GST_TYPE_RTP_ONVIF_PARSE \
  (gst_rtp_onvif_parse_get_type())
#define GST_RTP_ONVIF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_ONVIF_PARSE,GstRtpOnvifParse))
#define GST_RTP_ONVIF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_ONVIF_PARSE,GstRtpOnvifParseClass))
#define GST_IS_RTP_ONVIF_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_ONVIF_PARSE))
#define GST_IS_RTP_ONVIF_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_ONVIF_PARSE))

typedef struct _GstRtpOnvifParse GstRtpOnvifParse;
typedef struct _GstRtpOnvifParseClass GstRtpOnvifParseClass;

struct _GstRtpOnvifParse {
  GstElement element;

  /* pads */
  GstPad *sinkpad,*srcpad;
};

struct _GstRtpOnvifParseClass {
  GstElementClass parent_class;
};

GType gst_rtp_onvif_parse_get_type(void);
GST_ELEMENT_REGISTER_DECLARE (rtponvifparse);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GST_RTP_ONVIF_PARSE_H__ */
