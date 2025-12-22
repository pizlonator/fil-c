/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
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

#ifndef __GST_AVTP_CRF_SYNC_H__
#define __GST_AVTP_CRF_SYNC_H__

#include <gst/gst.h>

#include "gstavtpcrfbase.h"

G_BEGIN_DECLS
#define GST_TYPE_AVTP_CRF_SYNC (gst_avtp_crf_sync_get_type())
#define GST_AVTP_CRF_SYNC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVTP_CRF_SYNC,GstAvtpCrfSync))
#define GST_AVTP_CRF_SYNC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVTP_CRF_SYNC,GstAvtpCrfSyncClass))
#define GST_IS_AVTP_CRF_SYNC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVTP_CRF_SYNC))
#define GST_IS_AVTP_CRF_SYNC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVTP_CRF_SYNC))
typedef struct _GstAvtpCrfSync GstAvtpCrfSync;
typedef struct _GstAvtpCrfSyncClass GstAvtpCrfSyncClass;

struct _GstAvtpCrfSync
{
  GstAvtpCrfBase avtpcrfbase;
};

struct _GstAvtpCrfSyncClass
{
  GstAvtpCrfBaseClass parent_class;
};

GType gst_avtp_crf_sync_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (avtpcrfsync);

G_END_DECLS
#endif /* __GST_AVTP_CRF_SYNC_H__ */
