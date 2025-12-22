/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_AMC_INTERNAL_JNI_H__
#define __GST_AMC_INTERNAL_JNI_H__

#include "../gstamc-codec.h"
#include "../gstamc-format.h"

G_BEGIN_DECLS

struct _GstAmcFormat
{
  /* < private > */
  jobject object;               /* global reference */
};

typedef struct
{
  guint8 *data;
  gsize size;

  jobject object;               /* global reference */
} RealBuffer;

gboolean
gst_amc_buffer_get_position_and_limit (RealBuffer * buffer_, GError ** err,
    gint * position, gint * limit);

G_END_DECLS

#endif /* __GST_AMC_INTERNAL_JNI_H__ */
