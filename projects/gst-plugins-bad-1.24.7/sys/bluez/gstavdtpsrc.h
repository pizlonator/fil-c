/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012  Collabora Ltd.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __GST_AVDTP_SRC_H
#define __GST_AVDTP_SRC_H

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstavdtputil.h"
#include "gstavrcputil.h"

G_BEGIN_DECLS
#define GST_TYPE_AVDTP_SRC \
	(gst_avdtp_src_get_type())
#define GST_AVDTP_SRC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVDTP_SRC, GstAvdtpSrc))
#define GST_AVDTP_SRC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVDTP_SRC, GstAvdtpSrc))
#define GST_IS_AVDTP_SRC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVDTP_SRC))
#define GST_IS_AVDTP_SRC_CLASS(obj) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVDTP_SRC))
typedef struct _GstAvdtpSrc GstAvdtpSrc;
typedef struct _GstAvdtpSrcClass GstAvdtpSrcClass;

struct _GstAvdtpSrcClass
{
  GstBaseSrcClass parentclass;
};

struct _GstAvdtpSrc
{
  GstBaseSrc basesrc;

  GstAvdtpConnection conn;
  GstCaps *dev_caps;

  GstAvrcpConnection *avrcp;

  GstPoll *poll;
  GstPollFD pfd;
  gint unlocked;

  GstClockTime duration;

  guint transport_volume;
};

GType gst_avdtp_src_get_type (void);

G_END_DECLS
#endif
