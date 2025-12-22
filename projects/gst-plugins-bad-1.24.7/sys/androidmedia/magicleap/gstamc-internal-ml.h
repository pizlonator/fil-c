/*
 * Copyright (C) 2018 Collabora Ltd.
 *   Author: Xavier Claessens <xavier.claessens@collabora.com>
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

#ifndef __GST_AMC_INTERNAL_ML_H__
#define __GST_AMC_INTERNAL_ML_H__

#include "../gstamc-codec.h"
#include "../gstamc-format.h"
#include <ml_api.h>

G_BEGIN_DECLS

extern GstAmcFormatVTable gst_amc_format_ml_vtable;
extern GstAmcCodecVTable gst_amc_codec_ml_vtable;

GstAmcFormat *gst_amc_format_ml_new_handle (MLHandle handle);
MLHandle gst_amc_format_ml_get_handle (GstAmcFormat * format);

G_END_DECLS

#endif /* __GST_AMC_INTERNAL_ML_H__ */
